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


/**
 * @brief: Class that divides active world into a grid to quickly find entities. Application of this
 * in relation to the broad-narrow phase collision detection
 */
class SpatialPartitioning {
public:
    // cell sizes in float to determine the space size 
    // Tune this based on typical collider sizes / scene density.
    static constexpr float CELL_SIZE = 32.0f;

    // represents grid cords (x,y)
    struct CellCoord {
        int x, y;

    // operator to help this = other object cords
        bool operator==(const CellCoord& other) const { return x == other.x && y == other.y; }
    };

    // Hash function for CellCord so it can be used as unoredered_map key
    struct CellHash {
        size_t operator()(const CellCoord& c) const noexcept {
            
            // combine x and y cords into single hash
            // the XOR with shifted Y prevents collisions
            return std::hash<int>{}(c.x) ^ (std::hash<int>{}(c.y) << 1);
        }
    };



/**
 * @brief: Inserts entity into spatial grid where we calculate which grid cells
 * the entity overlaps.
 * 
 * example: Entity at (100,100) with radius 50 and Cellsize previously declared as 64
 * - overlaps cells: (0,0), (1,0),(0,1),(1,1)
 * - gets inserted into all those 4 cells 
 */
    void Insert(const ECS::Entity entity, const Vector3D& position, float radius) {
        //std::floor calculates the minimum value from float to int when you convert so basically
        // 15.75 -> becomes 15 instead of 16 so basically count down. 

        // calculate whats the spacial area minimum X and max X covers 
        // so like spacially you can imagine how the covered area fits 
        int minX = static_cast<int>(std::floor((position.X - radius) / CELL_SIZE));
        int maxX = static_cast<int>(std::floor((position.X + radius) / CELL_SIZE));

        // same thing as above but for the Y axis area expansion
        int minY = static_cast<int>(std::floor((position.Y - radius) / CELL_SIZE));
        int maxY = static_cast<int>(std::floor((position.Y + radius) / CELL_SIZE));

        // use for loop to iterate from minimum to maximum x/y cords for 3d and push it back
        // onto a grid (m_grid). so right now they will be placed onto a reference grid
        // to be accessed later for grid check into collision checks.
        for (int cx = minX; cx <= maxX; ++cx) {
            for (int cy = minY; cy <= maxY; ++cy) {
                m_grid[CellCoord{cx, cy}].push_back(entity);
            }
        }
    }

    void InsertBox(const ECS::Entity entity, const Vector3D& position, const Vector2D& halfExtents) {
        // Calculate the actual AABB bounds for the box
        float minX = position.X - halfExtents.X;
        float maxX = position.X + halfExtents.X;
        float minY = position.Y - halfExtents.Y;
        float maxY = position.Y + halfExtents.Y;

        // Convert to grid coordinates
        int cellMinX = static_cast<int>(std::floor(minX / CELL_SIZE));
        int cellMaxX = static_cast<int>(std::floor(maxX / CELL_SIZE));
        int cellMinY = static_cast<int>(std::floor(minY / CELL_SIZE));
        int cellMaxY = static_cast<int>(std::floor(maxY / CELL_SIZE));

        // Insert entity into all overlapping grid cells
        for (int cx = cellMinX; cx <= cellMaxX; ++cx) {
            for (int cy = cellMinY; cy <= cellMaxY; ++cy) {
                m_grid[CellCoord{ cx, cy }].push_back(entity);
            }
        }
    }

    // to access the internal grid 
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash>& Grid() { return m_grid; }

private:
    // the spatial grid: maps cell cordinates to list of entities in cell;
    // eg. m_grid[{0,1}] = [entity1, 2, etc]
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash> m_grid;
};

/**
 * @brief: MAIN physics update function
 * called every frame to:
 * 1. integrate velocities
 * 2. build spatial grid
 * 3. generate collision pairs
 * 4. resolve collisions 
 */

namespace ECS {

    /**
    * @brief Test circle-circle collision
    */
    bool TestCircleCircle(
        const Components::CircleCollider2D& circleA,
        const Components::LocalTransform& transformA,
        const Components::CircleCollider2D& circleB,
        const Components::LocalTransform& transformB,
        Vector2D& outNormal,
        float& outDepth)
    {
        // Calculate world-space centers
        Vector2D centerA(
            transformA.Position.X + circleA.Offset.X,
            transformA.Position.Y + circleA.Offset.Y
        );
        Vector2D centerB(
            transformB.Position.X + circleB.Offset.X,
            transformB.Position.Y + circleB.Offset.Y
        );

        // Collision test
        const float dx = centerB.X - centerA.X;
        const float dy = centerB.Y - centerA.Y;
        const float distSq = dx * dx + dy * dy;
        const float radiiSum = circleA.Radius + circleB.Radius;

        if (distSq >= radiiSum * radiiSum) {
            return false; // No collision
        }

        // Calculate normal and depth
        const float dist = std::sqrt(distSq);
        if (dist > 1e-6f) {
            outNormal = Vector2D(dx / dist, dy / dist);
        }
        else {
            outNormal = Vector2D(1.0f, 0.0f);
        }
        outDepth = radiiSum - dist;
        return true;
    }


    /**
     * @brief Test box-box collision using Collision utility
     */
    bool TestBoxBox(
        const Components::BoxCollider2D& boxA,
        const Components::LocalTransform& transformA,
        const Components::BoxCollider2D& boxB,
        const Components::LocalTransform& transformB,
        Vector2D& outNormal,
        float& outDepth)
    {
        Vector2D centerA(
            transformA.Position.X + boxA.Offset.X,
            transformA.Position.Y + boxA.Offset.Y
        );
        Vector2D centerB(
            transformB.Position.X + boxB.Offset.X,
            transformB.Position.Y + boxB.Offset.Y
        );

        Engine::Collision::AABB aabbA = Engine::Collision::MakeAABBCenterSize(
            centerA, boxA.HalfExtents * 2.0f
        );
        Engine::Collision::AABB aabbB = Engine::Collision::MakeAABBCenterSize(
            centerB, boxB.HalfExtents * 2.0f
        );

        Vector2D normal;
        float penetration;
        if (Engine::Collision::AABBvsAABB(aabbA, aabbB, &normal, &penetration)) {
            outNormal = normal;
            outDepth = penetration;
            return true;
        }

        return false;
    }


    /**
     * @brief Test circle-box collision using Collision utility
     */
    bool TestCircleBox(
        const Components::CircleCollider2D& circle,
        const Components::LocalTransform& circleTransform,
        const Components::BoxCollider2D& box,
        const Components::LocalTransform& boxTransform,
        Vector2D& outNormal,
        float& outDepth)
    {
        Vector2D circleCenter(
            circleTransform.Position.X + circle.Offset.X,
            circleTransform.Position.Y + circle.Offset.Y
        );
        Vector2D boxCenter(
            boxTransform.Position.X + box.Offset.X,
            boxTransform.Position.Y + box.Offset.Y
        );

        Engine::Collision::AABB aabb = Engine::Collision::MakeAABBCenterSize(
            boxCenter, box.HalfExtents * 2.0f
        );
        Engine::Collision::Circle circ;
        circ.Center = circleCenter;
        circ.Radius = circle.Radius;

        Engine::Collision::Manifold manifold;
        if (Engine::Collision::Overlap(circ, aabb, &manifold)) {
            outNormal = manifold.Normal;
            outDepth = manifold.Penetration;
            return true;
        }

        return false;
    }

    void PhysicsSystem::Update(World& world, const float dt) {
        //early exit if physics is disabled in specific scene
        if (!Engine::Physics::IsEnabled()) return;
        // early exit for invalid time stamps
        if (dt <= 0.0f) return;


        std::vector<Entity> dynamicEntities;
        dynamicEntities.reserve(512);

        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D,
            Components::AngularVelocity2D, Components::LocalTransform>(
                [&](const Entity entity, const Components::Rigidbody2D& rb,
                    Components::LinearVelocity2D& linearVel,
                    Components::AngularVelocity2D& angularVel,
                    Components::LocalTransform& transform) {

                        if (const auto* active = world.TryGet<Components::Active>(entity)) {
                            if (!active->Enabled) return;
                        }

                        if (rb.Mass <= 0.0f) return;

                        Vector2D acceleration = Engine::Physics::CalculateAcceleration(rb, linearVel);

                        if (rb.Flags & (1 << 1)) {
                            acceleration += Engine::Physics::GetGravity() * rb.GravityScale;
                        }

                        linearVel.Value += acceleration * dt;
                        transform.Position.X += linearVel.Value.X * dt;
                        transform.Position.Y += linearVel.Value.Y * dt;

                        if (!(rb.Flags & (1 << 2))) {
                            float angularAcceleration = Engine::Physics::CalculateAngularAcceleration(rb, angularVel);
                            if (std::abs(angularAcceleration * dt) > std::abs(angularVel.Value)) {
                                angularVel.Value = 0.0f;
                            }
                            else {
                                angularVel.Value += angularAcceleration * dt;
                            }
                            transform.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angularVel.Value * dt) * transform.Rotation;
                        }

                        if (Engine::Physics::IsWorldBoundsEnabled()) {
                            if (const auto* collider = world.TryGet<Components::CircleCollider2D>(entity)) {
                                Vector2D pos2D(transform.Position.X, transform.Position.Y);
                                Vector2D vel2D = linearVel.Value;

                                float entityRestitution = -1.0f;
                                if (const auto* mat = world.TryGet<Components::PhysicsMaterial2D>(entity)) {
                                    entityRestitution = mat->Restitution;
                                }

                                if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, collider->Radius,
                                    Engine::Physics::GetWorldBounds(),
                                    entityRestitution)) {
                                    transform.Position.X = pos2D.X;
                                    transform.Position.Y = pos2D.Y;
                                    linearVel.Value = vel2D;
                                }
                            }
                            else if (const auto* boxCollider = world.TryGet<Components::BoxCollider2D>(entity)) {
                                float approxRadius = std::max(boxCollider->HalfExtents.X, boxCollider->HalfExtents.Y);
                                Vector2D pos2D(transform.Position.X, transform.Position.Y);
                                Vector2D vel2D = linearVel.Value;

                                float entityRestitution = -1.0f;
                                if (const auto* mat = world.TryGet<Components::PhysicsMaterial2D>(entity)) {
                                    entityRestitution = mat->Restitution;
                                }

                                if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, approxRadius,
                                    Engine::Physics::GetWorldBounds(),
                                    entityRestitution)) {
                                    transform.Position.X = pos2D.X;
                                    transform.Position.Y = pos2D.Y;
                                    linearVel.Value = vel2D;
                                }
                            }
                        }

                        dynamicEntities.push_back(entity);
                });

        //collect static entities 
        std::vector<Entity> staticEntities;
        staticEntities.reserve(128);

        world.Each<Components::Rigidbody2D, Components::LocalTransform>(
            [&](const Entity entity, const Components::Rigidbody2D& rb,
                const Components::LocalTransform& transform) {

                    // Only process STATIC bodies (mass == 0)
                    if (rb.Mass > 0.0f) return;

                    // Check if entity is active
                    if (const auto* active = world.TryGet<Components::Active>(entity)) {
                        if (!active->Enabled) return;
                    }

                    // Must have a collider to participate in collisions
                    const bool hasCollider = world.Has<Components::CircleCollider2D>(entity) ||
                        world.Has<Components::BoxCollider2D>(entity);
                    if (!hasCollider) return;

                    staticEntities.push_back(entity);
            });

        SpatialPartitioning partition;
        for (const Entity entity : dynamicEntities) {
            const auto* t = world.TryGet<Components::LocalTransform>(entity);
            if (!t) continue;

            if (const auto* c = world.TryGet<Components::CircleCollider2D>(entity)) {
                partition.Insert(entity, t->Position, c->Radius);
            }
            else if (const auto* box = world.TryGet<Components::BoxCollider2D>(entity)) {
                partition.InsertBox(entity, t->Position, box->HalfExtents);
            }
        }

        // Add static entities to grid 
        for (const Entity entity : staticEntities) {
            const auto* t = world.TryGet<Components::LocalTransform>(entity);
            if (!t) continue;

            if (const auto* c = world.TryGet<Components::CircleCollider2D>(entity)) {
                partition.Insert(entity, t->Position, c->Radius);
            }
            else if (const auto* box = world.TryGet<Components::BoxCollider2D>(entity)) {
                partition.InsertBox(entity, t->Position, box->HalfExtents);
            }
        }

        std::vector<std::pair<Entity, Entity>> candidatePairs;
        std::unordered_set<uint64_t> pairSeen;

        candidatePairs.reserve(dynamicEntities.size() * 4);
        pairSeen.reserve(dynamicEntities.size() * 4);

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

        const int positionCorrectionIterations = 8;

        for (int iter = 0; iter < positionCorrectionIterations; ++iter) {
            int collisionsResolved = 0;

            for (const auto& pr : candidatePairs) {
                const Entity A = pr.first;
                const Entity B = pr.second;

                if (!world.IsAlive(A) || !world.IsAlive(B)) continue;

                auto* tA_ptr = world.TryGet<Components::LocalTransform>(A);
                auto* tB_ptr = world.TryGet<Components::LocalTransform>(B);
                if (!tA_ptr || !tB_ptr) continue;

                if (const auto* activeA = world.TryGet<Components::Active>(A)) {
                    if (!activeA->Enabled) continue;
                }
                if (const auto* activeB = world.TryGet<Components::Active>(B)) {
                    if (!activeB->Enabled) continue;
                }

                const auto* circleA = world.TryGet<Components::CircleCollider2D>(A);
                const auto* boxA = world.TryGet<Components::BoxCollider2D>(A);
                const auto* circleB = world.TryGet<Components::CircleCollider2D>(B);
                const auto* boxB = world.TryGet<Components::BoxCollider2D>(B);

                if ((!circleA && !boxA) || (!circleB && !boxB)) {
                    continue;
                }

                bool hasPhysicsA = world.TryGet<Components::Rigidbody2D>(A) &&
                    world.TryGet<Components::LinearVelocity2D>(A);
                bool hasPhysicsB = world.TryGet<Components::Rigidbody2D>(B) &&
                    world.TryGet<Components::LinearVelocity2D>(B);

                if (!hasPhysicsA && !hasPhysicsB) {
                    continue;
                }

                Vector2D normal;
                float depth;
                bool collided = false;

                if (circleA && circleB) {
                    collided = TestCircleCircle(*circleA, *tA_ptr, *circleB, *tB_ptr, normal, depth);
                }
                else if (boxA && boxB) {
                    collided = TestBoxBox(*boxA, *tA_ptr, *boxB, *tB_ptr, normal, depth);
                }
                else if (circleA && boxB) {
                    collided = TestCircleBox(*circleA, *tA_ptr, *boxB, *tB_ptr, normal, depth);
                }
                else if (boxA && circleB) {
                    collided = TestCircleBox(*circleB, *tB_ptr, *boxA, *tA_ptr, normal, depth);
                    if (collided) {
                        normal = -normal;  // Negate normal since we swapped the entities
                    }
                }

                if (!collided) continue;

                collisionsResolved++;

                Components::Rigidbody2D rbA{ 0 }, rbB{ 0 };
                Components::LinearVelocity2D velA{ {0,0} }, velB{ {0,0} };

                const auto* rbA_ptr = world.TryGet<Components::Rigidbody2D>(A);
                const auto* rbB_ptr = world.TryGet<Components::Rigidbody2D>(B);
                auto* velA_ptr = world.TryGet<Components::LinearVelocity2D>(A);
                auto* velB_ptr = world.TryGet<Components::LinearVelocity2D>(B);

                bool hasRbA = rbA_ptr != nullptr;
                bool hasRbB = rbB_ptr != nullptr;
                bool hasVelA = velA_ptr != nullptr;
                bool hasVelB = velB_ptr != nullptr;

                if (hasRbA) rbA = *rbA_ptr;
                if (hasRbB) rbB = *rbB_ptr;
                if (hasVelA) velA = *velA_ptr;
                if (hasVelB) velB = *velB_ptr;

                Components::PhysicsMaterial2D matA{ 0.2f, 0.5f, 0.5f };
                Components::PhysicsMaterial2D matB{ 0.2f, 0.5f, 0.5f };

                if (const auto* matA_ptr = world.TryGet<Components::PhysicsMaterial2D>(A)) {
                    matA = *matA_ptr;
                }
                if (const auto* matB_ptr = world.TryGet<Components::PhysicsMaterial2D>(B)) {
                    matB = *matB_ptr;
                }

                Components::PhysicsMaterial2D combinedMat{
                    (matA.Friction + matB.Friction) * 0.5f,
                    std::max(matA.Restitution, matB.Restitution),
                    (matA.PositionCorrectPercent + matB.PositionCorrectPercent) * 0.5f
                };

                auto& tA = *tA_ptr;
                auto& tB = *tB_ptr;

                Engine::Physics::ResolveCollision(
                    rbA, rbB,
                    velA, velB,
                    tA, tB,
                    normal, depth,
                    combinedMat
                );

                if (hasVelA && velA_ptr) *velA_ptr = velA;
                if (hasVelB && velB_ptr) *velB_ptr = velB;
            }

            if (collisionsResolved == 0) break;
        }
    }

} // namespace ECS
