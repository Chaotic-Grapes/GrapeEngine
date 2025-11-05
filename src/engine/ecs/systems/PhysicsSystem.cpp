#include "ecs/systems/PhysicsSystem.h"
#include "services/Time.h"
#include "physics/Collision.h"
#include "ecs/Entity.h"
#include <services/OverlayService.h>
#include "physics/Physics.h"
#include "ecs/Components.h"
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <helpers/MathUtils.h>

// --- Simple 2D grid-based spatial partitioning for broad-phase collision ---
class SpatialPartitioning {
public:
    // Cell size determines granularity; tweak as needed
    // Should be large enough to cover typical entity sizes and their velocities
    static constexpr float CELL_SIZE = 200.0f;

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
                CellCoord cell{ baseCell.x + dx, baseCell.y + dy };
                auto it = m_grid.find(cell);
                if (it != m_grid.end()) {
                    result.insert(result.end(), it->second.begin(), it->second.end());
                }
            }
        }

        return result;
    }

private:
    // Simple struct for grid cell coordinates
    struct CellCoord {
        int x, y;
        
        bool operator==(const CellCoord& other) const {
            return x == other.x && y == other.y;
        }
    };
    
    // Hash functor for CellCoord
    struct CellHash {
        size_t operator()(const CellCoord& cell) const noexcept {
            // Combine hash values using bit shifting
            return std::hash<int>{}(cell.x) ^ (std::hash<int>{}(cell.y) << 1);
        }
    };

    // Map from cell coordinates to entities
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash> m_grid;

    // Convert world position to grid cell
    CellCoord _getCell(const Vector3D& position) const {
        int x = static_cast<int>(std::floor(position.X / CELL_SIZE));
        int y = static_cast<int>(std::floor(position.Y / CELL_SIZE));
        return CellCoord{ x, y };
    }
};

namespace ECS {
    void PhysicsSystem::Update(World& world, const float dt) {
        if (!Engine::Physics::IsEnabled()) return;

        // 2 instances when physics should run:
        // 1) Playing (IsGamePlaying() == true, IsStepRequested() == false) 
        // 2) Paused + STEP (IsGamePlaying() == false, IsStepRequested() == true)
        auto* overlay = world.GetSystem<Overlay>();
        if (!overlay) {
            return; 
        }

        // 1) Physics runs when m_gameState == Playing
        // So if m_gameState == Paused for e.g. it just freezes the frame (somehow it all works out)
        if (!overlay->IsGamePlaying() && !overlay->IsStepRequested()) {
            return;
        }

        // 2) Physics runs when m_gameState == Paused + STEP (step-by-step physics mode)
        // Clear step flag if set (1 frame)
        if (overlay->IsStepRequested()) {
            overlay->ClearStepRequest();
        }

        // Broad-phase spatial partitioning (e.g., quadtree or grid)
        // This will optimize collision checks by reducing the number of pairs to test
        SpatialPartitioning partitioning;
        world.Each<Components::LocalTransform, Components::CircleCollider2D>(
            [&](const Entity entity, const Components::LocalTransform& transform, const Components::CircleCollider2D& collider) {
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }
                
                // Use collider center for partitioning
                const Vector3D colliderCenter = transform.Position + Vector3D(collider.Offset.X, collider.Offset.Y, 0.0f);
                partitioning.Insert(entity, colliderCenter, collider.Radius);
            });

        // Iterate over all entities with Rigidbody2D, LinearVelocity2D, AngularVelocity2D, and LocalTransform
        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::AngularVelocity2D, Components::LocalTransform>(
            [&](const Entity entity, const Components::Rigidbody2D& rb, Components::LinearVelocity2D& linearVel, const Components::AngularVelocity2D& angularVel, Components::LocalTransform& transform) {
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }
                
                if (rb.Mass <= 0.0f) return; // Skip static bodies

                // Apply forces and gravity
                Vector2D acceleration = Engine::Physics::CalculateAcceleration(rb, linearVel);
                if (rb.Flags & (1 << 1)) { // UseGravity is bit 1
                    acceleration += Engine::Physics::GetGravity() * rb.GravityScale;
                }
                linearVel.Value += acceleration * dt;

                // Calculate intended position
                const Vector2D intendedPos = Vector2D(transform.Position.X, transform.Position.Y) + linearVel.Value * dt;

                // Update position and rotation BEFORE collision detection
                transform.Position.X = intendedPos.X;
                transform.Position.Y = intendedPos.Y;
                if (!(rb.Flags & (1 << 2))) { // FixedRotation is bit 2
                    transform.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angularVel.Value * dt) * transform.Rotation;
                }

                // Apply world boundary constraints if enabled
                if (Engine::Physics::IsWorldBoundsEnabled() && world.Has<Components::CircleCollider2D>(entity)) {
                    const auto& collider = world.Get<Components::CircleCollider2D>(entity);
                    Vector2D pos2D(transform.Position.X, transform.Position.Y);
                    Vector2D vel2D = linearVel.Value;
                    
                    // Get entity's restitution from PhysicsMaterial2D if it has one
                    float entityRestitution = -1.0f;  // -1 means use default bounds restitution
                    if (world.Has<Components::PhysicsMaterial2D>(entity)) {
                        entityRestitution = world.Get<Components::PhysicsMaterial2D>(entity).Restitution;
                    }
                    
                    if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, collider.Radius, Engine::Physics::GetWorldBounds(), entityRestitution)) {
                        transform.Position.X = pos2D.X;
                        transform.Position.Y = pos2D.Y;
                        linearVel.Value = vel2D;
                    }
                }

                // Process collision if entity has a CircleCollider2D
                if (world.Has<Components::CircleCollider2D>(entity)) {
                    // Collider center for broad-phase
                    const auto& collider = world.Get<Components::CircleCollider2D>(entity);
                    const Vector3D colliderCenter = transform.Position + Vector3D(collider.Offset.X, collider.Offset.Y, 0.0f);

                    const auto potentialCollisions = partitioning.Query(colliderCenter, collider.Radius);

                    // Narrow-phase collision resolution
                    for (const auto& otherEntity : potentialCollisions) {
                        if (entity == otherEntity) continue;

                        if (!world.Has<Components::LocalTransform>(otherEntity) || 
                            !world.Has<Components::CircleCollider2D>(otherEntity)) continue;

                        // Skip if other entity doesn't have rigidbody or velocity components
                        if (!world.Has<Components::Rigidbody2D>(otherEntity) ||
                            !world.Has<Components::LinearVelocity2D>(otherEntity)) continue;

                        auto& otherTransform = world.Get<Components::LocalTransform>(otherEntity);
                        const auto& otherCollider = world.Get<Components::CircleCollider2D>(otherEntity);
                        const auto& otherRb = world.Get<Components::Rigidbody2D>(otherEntity);
                        auto& otherVel = world.Get<Components::LinearVelocity2D>(otherEntity);

                        // Get physics materials from both entities and combine them
                        Components::PhysicsMaterial2D physicsMat1{0.2f, 0.5f, 0.2f};
                        Components::PhysicsMaterial2D physicsMat2{0.2f, 0.5f, 0.2f};
                        
                        if (world.Has<Components::PhysicsMaterial2D>(entity)) {
                            physicsMat1 = world.Get<Components::PhysicsMaterial2D>(entity);
                        }
                        if (world.Has<Components::PhysicsMaterial2D>(otherEntity)) {
                            physicsMat2 = world.Get<Components::PhysicsMaterial2D>(otherEntity);
                        }
                        
                        // Combine physics materials: average friction, max restitution, average position correction
                        Components::PhysicsMaterial2D physicsMat{
                            (physicsMat1.Friction + physicsMat2.Friction) * 0.5f,
                            std::max(physicsMat1.Restitution, physicsMat2.Restitution),
                            (physicsMat1.PositionCorrectPercent + physicsMat2.PositionCorrectPercent) * 0.5f
                        };

                        // Resolve circle-circle collision with offsets
                        Engine::Physics::ResolveCircleCircleCollision(
                            rb, otherRb,
                            linearVel, otherVel,
                            transform, otherTransform,
                            collider.Radius, otherCollider.Radius,
                            collider.Offset, otherCollider.Offset,
                            physicsMat
                        );
                    }
                }
            });
    }
}
