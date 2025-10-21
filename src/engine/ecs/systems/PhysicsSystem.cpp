#include "ecs/systems/PhysicsSystem.h"
#include "services/Time.h"
#include "physics/Collision.h"
#include "physics/Physics.h"
#include "ecs/Components.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <algorithm>
#include "helpers/MathUtils.h"
#include "helpers/EntityUtils.h"
#include <iostream>

class SpatialPartitioning {
public:
    // Smaller cell size improves accuracy at the cost of more cells.
    // Tune this based on typical collider sizes / scene density.
    static constexpr float CELL_SIZE = 32.0f;

    struct CellCoord {
        int x, y;
        bool operator==(const CellCoord& other) const { return x == other.x && y == other.y; }
    };

    struct CellHash {
        size_t operator()(const CellCoord& c) const noexcept {
            // combine coordinates
            return std::hash<int>{}(c.x) ^ (std::hash<int>{}(c.y) << 1);
        }
    };

    // Insert entity into all cells overlapped by position +/- radius
    void Insert(const ECS::Entity entity, const Vector3D& position, float radius) {
        int minX = static_cast<int>(std::floor((position.X - radius) / CELL_SIZE));
        int maxX = static_cast<int>(std::floor((position.X + radius) / CELL_SIZE));
        int minY = static_cast<int>(std::floor((position.Y - radius) / CELL_SIZE));
        int maxY = static_cast<int>(std::floor((position.Y + radius) / CELL_SIZE));

        for (int cx = minX; cx <= maxX; ++cx) {
            for (int cy = minY; cy <= maxY; ++cy) {
                m_grid[CellCoord{cx, cy}].push_back(entity);
            }
        }
    }

    // Access grid for pair generation
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash>& Grid() { return m_grid; }

private:
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash> m_grid;
};

namespace ECS {

    void PhysicsSystem::Update(World& world, const float dt) {
        if (!Engine::Physics::IsEnabled()) return;
        if (dt <= 0.0f) return;

        const int substeps = 3;  // Run physics 3 times per frame
        const float subDt = dt / static_cast<float>(substeps);
        // 1) Integrate velocities -> update positions & rotations for all dynamic bodies
        std::vector<Entity> dynamicEntities;
        dynamicEntities.reserve(512);

        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::AngularVelocity2D, Components::LocalTransform>(
            [&](const Entity entity, const Components::Rigidbody2D& rb, Components::LinearVelocity2D& linearVel, const Components::AngularVelocity2D& angularVel, Components::LocalTransform& transform) {
                // Skip disabled entities
                if (world.Has<Components::Active>(entity) && !world.Get<Components::Active>(entity).Enabled) return;
                if (rb.Mass <= 0.0f) return; // static bodies don't integrate here

                if (linearVel.Value.Length() < 0.05f) {
                    return;  // Don't integrate, don't add to dynamicEntities
                }

                // Apply forces & gravity
                Vector2D acceleration = Engine::Physics::CalculateAcceleration(rb, linearVel);
                if (rb.Flags & (1 << 1)) { acceleration += Engine::Physics::GetGravity() * rb.GravityScale; }
                linearVel.Value += acceleration * dt;

                // Integrate position and rotation (semi-explicit Euler)
                transform.Position.X += linearVel.Value.X * dt;
                transform.Position.Y += linearVel.Value.Y * dt;

                if (!(rb.Flags & (1 << 2))) { // not fixed rotation
                    transform.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angularVel.Value * dt) * transform.Rotation;
                }

                // World bounds constraint (keep integrated pos in sync)
                if (Engine::Physics::IsWorldBoundsEnabled() && world.Has<Components::CircleCollider2D>(entity)) {
                    const auto& collider = world.Get<Components::CircleCollider2D>(entity);
                    Vector2D pos2D(transform.Position.X, transform.Position.Y);
                    Vector2D vel2D = linearVel.Value;

                    float entityRestitution = -1.0f;
                    if (world.Has<Components::PhysicsMaterial2D>(entity)) {
                        entityRestitution = world.Get<Components::PhysicsMaterial2D>(entity).Restitution;
                    }

                    if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, collider.Radius, Engine::Physics::GetWorldBounds(), entityRestitution)) {
                        transform.Position.X = pos2D.X;
                        transform.Position.Y = pos2D.Y;
                        linearVel.Value = vel2D;
                    }
                }

                dynamicEntities.push_back(entity);
            });

        // 2) Build spatial partition from up-to-date positions (include static and dynamic circle colliders)
        SpatialPartitioning partition;
        partition.Grid().reserve(1024); // hint (implementation dependent)
        world.Each<Components::LocalTransform, Components::CircleCollider2D>([&](const Entity entity, const Components::LocalTransform& transform, const Components::CircleCollider2D& collider) {
            if (world.Has<Components::Active>(entity) && !world.Get<Components::Active>(entity).Enabled) return;
            const Vector3D center = transform.Position + Vector3D(collider.Offset.X, collider.Offset.Y, 0.0f);
            partition.Insert(entity, center, collider.Radius);
        });

        // 3) Generate unique candidate pairs from cells (avoid duplicate pairs)
        std::vector<std::pair<Entity, Entity>> candidatePairs;
        candidatePairs.reserve(1024);
        std::unordered_set<uint64_t> pairSeen;
        pairSeen.reserve(2048);

        // helper to create a stable 64-bit pair key (order smaller->larger)
        auto makePairKey = [](uint64_t a, uint64_t b) -> uint64_t {
            if (a > b) std::swap(a, b);
            return (a << 32) | (b & 0xFFFFFFFFull);
        };

        for (const auto& cellEntry : partition.Grid()) {
            const auto& entities = cellEntry.second;
            const size_t n = entities.size();
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    const uint64_t a = ECS::EntityUtils::Pack(entities[i]);
                    const uint64_t b = ECS::EntityUtils::Pack(entities[j]);
                    const uint64_t key = makePairKey(a, b);
                    if (pairSeen.insert(key).second) {
                        candidatePairs.emplace_back(entities[i], entities[j]);
                    }
                }
            }
        }

        // 4) Narrow-phase: precise checks + resolve collisions once per unique pair
        // Use a small number of solver iterations for position correction to reduce interpenetration
        const int positionCorrectionIterations = 4;

        for (int iter = 0; iter < positionCorrectionIterations; ++iter) {
            int collisionsResolved = 0;
            for (const auto& pr : candidatePairs) {
                const Entity A = pr.first;
                const Entity B = pr.second;

                // Validate entities and required components
                if (!world.IsAlive(A) || !world.IsAlive(B)) continue;
                if (!world.Has<Components::LocalTransform>(A) || !world.Has<Components::LocalTransform>(B)) continue;
                if (!world.Has<Components::CircleCollider2D>(A) || !world.Has<Components::CircleCollider2D>(B)) continue;

                // Skip disabled actives
                if (world.Has<Components::Active>(A) && !world.Get<Components::Active>(A).Enabled) continue;
                if (world.Has<Components::Active>(B) && !world.Get<Components::Active>(B).Enabled) continue;

                // If neither has a rigidbody/velocity, skip (no dynamic response)
                if ((!world.Has<Components::Rigidbody2D>(A) || !world.Has<Components::LinearVelocity2D>(A)) &&
                    (!world.Has<Components::Rigidbody2D>(B) || !world.Has<Components::LinearVelocity2D>(B))) {
                    continue;
                }

                auto& tA = world.Get<Components::LocalTransform>(A);
                auto& tB = world.Get<Components::LocalTransform>(B);
                const auto& cA = world.Get<Components::CircleCollider2D>(A);
                const auto& cB = world.Get<Components::CircleCollider2D>(B);

                // compute world-space centers
                const Vector3D centerA3 = tA.Position + Vector3D(cA.Offset.X, cA.Offset.Y, 0.0f);
                const Vector3D centerB3 = tB.Position + Vector3D(cB.Offset.X, cB.Offset.Y, 0.0f);
                const Vector2D centerA{ centerA3.X, centerA3.Y };
                const Vector2D centerB{ centerB3.X, centerB3.Y };

                const float rA = cA.Radius;
                const float rB = cB.Radius;

                // quick reject
                const float dx = centerB.X - centerA.X;
                const float dy = centerB.Y - centerA.Y;
                const float distSq = dx*dx + dy*dy;
                const float radii = rA + rB;
                if (distSq >= radii * radii) continue;
                collisionsResolved++;
                // prepare components for resolution; ensure required components exist when calling resolve
                // Provide default rigidbodies/materials for static-like objects if missing
                Components::Rigidbody2D rbA{ 0 }, rbB{ 0 };
                Components::LinearVelocity2D velA{ {0,0} }, velB{ {0,0} };

                bool hasRbA = world.Has<Components::Rigidbody2D>(A);
                bool hasRbB = world.Has<Components::Rigidbody2D>(B);
                bool hasVelA = world.Has<Components::LinearVelocity2D>(A);
                bool hasVelB = world.Has<Components::LinearVelocity2D>(B);

                if (hasRbA) rbA = world.Get<Components::Rigidbody2D>(A);
                if (hasRbB) rbB = world.Get<Components::Rigidbody2D>(B);
                if (hasVelA) velA = world.Get<Components::LinearVelocity2D>(A);
                if (hasVelB) velB = world.Get<Components::LinearVelocity2D>(B);

                // Physics materials - combine
                Components::PhysicsMaterial2D matA{ 0.2f, 0.5f, 0.5f }, matB{ 0.2f, 0.5f, 0.5f };
                if (world.Has<Components::PhysicsMaterial2D>(A)) matA = world.Get<Components::PhysicsMaterial2D>(A);
                if (world.Has<Components::PhysicsMaterial2D>(B)) matB = world.Get<Components::PhysicsMaterial2D>(B);

                Components::PhysicsMaterial2D combinedMat{
                    (matA.Friction + matB.Friction) * 0.5f,
                    std::max(matA.Restitution, matB.Restitution),
                    (matA.PositionCorrectPercent + matB.PositionCorrectPercent) * 0.5f
                };

                // Resolve via engine helper (engine does impulse + positional correction)
                Engine::Physics::ResolveCircleCircleCollision(
                    rbA, rbB,
                    velA, velB,
                    tA, tB,
                    rA, rB,
                    cA.Offset, cB.Offset,
                    combinedMat
                );

                // write back velocities if they existed
                if (hasVelA) world.Get<Components::LinearVelocity2D>(A) = velA;
                if (hasVelB) world.Get<Components::LinearVelocity2D>(B) = velB;
                // transforms are updated in-place (tA, tB)
            } // for candidatePairs
            if (collisionsResolved == 0) break;


        } // iterations
#ifdef _DEBUG
        static int frameCount = 0;
        static float totalTime = 0.0f;
        static int totalPairs = 0;
        static int totalEntities = 0;

        frameCount++;
        totalTime += dt;
        totalPairs += candidatePairs.size();
        totalEntities += dynamicEntities.size();

        if (frameCount >= 60) {
            std::cout << "\n=== Physics Stats (60 frames avg) ===\n"
                << "  Dynamic Entities: " << (totalEntities / 60) << "\n"
                << "  Grid Cells Used: " << partition.Grid().size() << "\n"
                << "  Candidate Pairs: " << (totalPairs / 60) << "\n"
                << "  Avg Frame Time: " << ((totalTime / 60.0f) * 1000.0f) << "ms\n"
                << "======================================\n";

            frameCount = 0;
            totalTime = 0.0f;
            totalPairs = 0;
            totalEntities = 0;
        }
#endif
        // 5) (Optional) Additional collision handling per-entity (e.g., callbacks) can be placed here.
    }


} // namespace ECS

