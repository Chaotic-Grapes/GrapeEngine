#include "ecs/systems/PhysicsSystem.h"
#include "services/Time.h"
#include "physics/Collision.h"
#include "physics/Physics.h"
#include "ecs/Components.h"
#include <unordered_map>
#include <vector>
#include <cmath>
#include <tuple>
#include <helpers/MathUtils.h>

// --- Simple 2D grid-based spatial partitioning for broad-phase collision ---
class SpatialPartitioning {
public:
    // Cell size determines granularity; tweak as needed
    static constexpr float CELL_SIZE = 2.0f;

    // Insert an entity into the grid
    void Insert(const ECS::Entity entity, const Vector3D& position, float radius) {
        const auto cell = _getCell(position);
        m_grid[cell].push_back(entity);
    }

    // Query for entities near a position (returns possible collision candidates)
    std::vector<ECS::Entity> Query(const Vector3D& position, float radius) const {
        std::vector<ECS::Entity> result;
        const auto baseCell = _getCell(position);

        // Check neighboring cells (including the cell itself)
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                auto cell = std::make_tuple(std::get<0>(baseCell) + dx, std::get<1>(baseCell) + dy);
                auto it = m_grid.find(cell);
                if (it != m_grid.end()) {
                    result.insert(result.end(), it->second.begin(), it->second.end());
                }
            }
        }

        return result;
    }

private:
    // Map from cell coordinates to entities
    std::unordered_map<std::tuple<int, int>, std::vector<ECS::Entity>> m_grid;

    // Convert world position to grid cell
    std::tuple<int, int> _getCell(const Vector3D& position) const {
        int x = static_cast<int>(std::floor(position.X / CELL_SIZE));
        int y = static_cast<int>(std::floor(position.Y / CELL_SIZE));

        return std::make_tuple(x, y);
    }
};

namespace ECS {
    void PhysicsSystem::Update(World& world, const float dt) {
        if (!Engine::Physics::IsEnabled()) return;

        // Broad-phase spatial partitioning (e.g., quadtree or grid)
        // This will optimize collision checks by reducing the number of pairs to test
        SpatialPartitioning partitioning;
        world.Each<Components::LocalTransform, Components::CircleCollider2D>(
            [&](const Entity entity, const Components::LocalTransform& transform, const Components::CircleCollider2D& collider) {
                // Use collider center for partitioning
                const Vector3D colliderCenter = transform.Position + Vector3D(collider.Offset.X, collider.Offset.Y, 0.0f);
                partitioning.Insert(entity, colliderCenter, collider.Radius);
            });

        // Iterate over all entities with Rigidbody2D, LinearVelocity2D, AngularVelocity2D, and LocalTransform
        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::AngularVelocity2D, Components::LocalTransform>(
            [&](const Entity entity, const Components::Rigidbody2D& rb, Components::LinearVelocity2D& linearVel, const Components::AngularVelocity2D& angularVel, Components::LocalTransform& transform) {
                if (rb.Mass <= 0.0f) return; // Skip static bodies

                // Calculate intended position
                const Vector2D intendedPos = Vector2D(transform.Position.X, transform.Position.Y) + linearVel.Value * dt;

                // Apply forces and gravity
                Vector2D acceleration = Engine::Physics::CalculateAcceleration(rb);
                if (rb.Flags & (1 << 1)) { // UseGravity is bit 1
                    acceleration += Engine::Physics::GetGravity() * rb.GravityScale;
                }
                linearVel.Value += acceleration * dt;

                // Collider center for broad-phase
                const auto& collider = world.Get<Components::CircleCollider2D>(entity);
                const Vector3D colliderCenter = transform.Position + Vector3D(collider.Offset.X, collider.Offset.Y, 0.0f);

                const auto potentialCollisions = partitioning.Query(colliderCenter, collider.Radius);

                // Narrow-phase collision resolution
                for (const auto& otherEntity : potentialCollisions) {
                    if (entity == otherEntity) continue;

                    const auto& otherTransform = world.Get<Components::LocalTransform>(otherEntity);
                    const auto& otherCollider = world.Get<Components::CircleCollider2D>(otherEntity);

                    const Vector2D centerA = Vector2D(colliderCenter.X, colliderCenter.Y);
                    const Vector2D centerB = Vector2D(
                        otherTransform.Position.X + otherCollider.Offset.X,
                        otherTransform.Position.Y + otherCollider.Offset.Y
                    );

                    Engine::Collision::Circle circleA{ centerA, collider.Radius };
                    Engine::Collision::Circle circleB{ centerB, otherCollider.Radius };

                    Engine::Collision::Manifold manifold;
                    if (Engine::Collision::Overlap(circleA, circleB, &manifold)) {
                        // Resolve collision (implement your own response here)
                        // Example: separate entities, adjust velocities, etc.
                    }
                }

                // Update position and rotation
                transform.Position.X = intendedPos.X;
                transform.Position.Y = intendedPos.Y;
                if (!(rb.Flags & (1 << 2))) { // FixedRotation is bit 2
                    transform.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angularVel.Value * dt) * transform.Rotation;
                }
            });
    }
}
