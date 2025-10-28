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

    void PhysicsSystem::Update(World& world, const float dt) {
        //early exit if physics is disabled in specific scene
        if (!Engine::Physics::IsEnabled()) return;
        // early exit for invalid time stamps
        if (dt <= 0.0f) return;

        // substeps cuts down pacing for faster moving objects so collision
        // checking is more accurate for fast moving objects
        const int substeps = 3;  // Run physics 3 times per frame
        // basically per delta time is slowed down 3 times 
        const float subDt = dt / static_cast<float>(substeps);

        // where we integrate velocities where positions and rotations are updated 
        // based on velocities, this is the physics simulation part. 

        std::vector<Entity> dynamicEntities;

        // pre-allocate to help with performance 
        dynamicEntities.reserve(512);

        // processes entities with physics components 
        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::AngularVelocity2D, Components::LocalTransform>(
            [&](const Entity entity, const Components::Rigidbody2D& rb, Components::LinearVelocity2D& linearVel, const Components::AngularVelocity2D& angularVel, Components::LocalTransform& transform) {

                // Skip disabled entities
                if (const auto* active = world.TryGet<Components::Active>(entity)) {
                    if (!active->Enabled) return;
                }

                // Skip static bodies (zero or negative mass)
                if (rb.Mass <= 0.0f)
                    return;

                // Apply forces & gravity (f = ma in a = f/m)
                Vector2D acceleration = Engine::Physics::CalculateAcceleration(rb, linearVel);

                // add gravity if enabled (basically bit 1 of flag)
                if (rb.Flags & (1 << 1)) { acceleration += Engine::Physics::GetGravity() * rb.GravityScale; }

                // update velocity: v = velocity + aceleration * dt (euler's integration)
                linearVel.Value += acceleration * dt;

                // Integrate position and rotation (semi-explicit Euler)
                // x position = x linear value * dt
                transform.Position.X += linearVel.Value.X * dt;
                transform.Position.Y += linearVel.Value.Y * dt;

                //part where rotation is integrated

                // skip if rotation is locked 
                if (!(rb.Flags & (1 << 2))) {
                    // convert angular velocity to quaternion rotation
                    transform.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angularVel.Value * dt) * transform.Rotation;
                }

                // World bounds constraint 
                // keep entities inside world boundaries( bounce off edges)
                if (Engine::Physics::IsWorldBoundsEnabled()) {
                    if (const auto* collider = world.TryGet<Components::CircleCollider2D>(entity)) {
                        Vector2D pos2D(transform.Position.X, transform.Position.Y);
                        Vector2D vel2D = linearVel.Value;

                        // entities bounciess (restitution set)
                        float entityRestitution = -1.0f;
                        if (const auto* mat = world.TryGet<Components::PhysicsMaterial2D>(entity)) {
                            entityRestitution = mat->Restitution;
                        }

                        // check and apply boundary constraints
                        // returns true if entity hits a boundary
                        if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, collider->Radius, Engine::Physics::GetWorldBounds(), entityRestitution)) {
                            transform.Position.X = pos2D.X;
                            transform.Position.Y = pos2D.Y;
                            linearVel.Value = vel2D;
                        }
                    }
                }

                // track entity for collision detection
                dynamicEntities.push_back(entity);
            });

        // phase 2 of broadphase to build spatial grid where we actively 
        // insert all static and dynamitc entities into grid
        SpatialPartitioning partition;
        partition.Grid().reserve(1024);

        // insert all entities with circle colliders into grid 
        world.Each<Components::LocalTransform, Components::CircleCollider2D>([&](const Entity entity, const Components::LocalTransform& transform, const Components::CircleCollider2D& collider) {

            // if entity is not active return
            if (const auto* active = world.TryGet<Components::Active>(entity)) {
                if (!active->Enabled) return;
            }

            //calculate world-space center (position + collider offset)
            const Vector3D center = transform.Position + Vector3D(collider.Offset.X, collider.Offset.Y, 0.0f);

            // insert into spatial grid using bounding radius
            partition.Insert(entity, center, collider.Radius);
            });

        // generate candidate pairs in this part to check entities within same cells
        // to find potential collisions only entities in same/nearby cells are considered
        std::vector<std::pair<Entity, Entity>> candidatePairs;
        candidatePairs.reserve(1024);

        // track which pairs we have already checked to prevent dupes
        // eg if we check (A,B), we don't want to check (B,A).
        std::unordered_set<uint64_t> pairSeen;
        // preallocate for performance 
        pairSeen.reserve(2048);

        // lambda helper to create a unique key for entity pairs 
        // always orderes smaller entitiy ID first 
        auto makePairKey = [](uint64_t a, uint64_t b) -> uint64_t {
            // ensure a<= b
            if (a > b) std::swap(a, b);
            // combines into a 64 bit key
            return (a << 32) | (b & 0xFFFFFFFFull);
            };

        //iterate through all grid cells
        for (const auto& cellEntry : partition.Grid()) {
            const auto& entities = cellEntry.second;
            const size_t n = entities.size();

            // check unique pairs in cell
            //eg. cell has (A, B, C) 
            // possibilities -> (A,C)(B,C)(A,B)
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {

                    // packs entities into 64-bit IDs
                    const uint64_t a = ECS::EntityUtils::Pack(entities[i]);
                    const uint64_t b = ECS::EntityUtils::Pack(entities[j]);
                    const uint64_t key = makePairKey(a, b);

                    //only add if we have not seen this pair before
                    // insert() returns {iterator, true} if inserted,{iterator, false} if already existe
                    if (pairSeen.insert(key).second) {
                        candidatePairs.emplace_back(entities[i], entities[j]);
                    }
                }
            }
        }

        // Narrow phase kicking in here - more precise collision checking
        // for each candidate pair 
        // 1) collision test based on distance
        // 2) resolve collision with impulse and position correction in mind

        //using this as a iteration counter the higher the more accurate since it loops more
        const int positionCorrectionIterations = 4;

        for (int iter = 0; iter < positionCorrectionIterations; ++iter) {

            // tracker to be used if early exits
            int collisionsResolved = 0;

            // process each candidate pair
            for (const auto& pr : candidatePairs) {
                const Entity A = pr.first;
                const Entity B = pr.second;

                // check if either A or B are alive objs
                if (!world.IsAlive(A) || !world.IsAlive(B)) continue;

                // get transform and collider for both entities
                auto [tA_ptr, cA_ptr] = world.TryGetComponents
                    <Components::LocalTransform,
                    Components::CircleCollider2D>(A);
                auto [tB_ptr, cB_ptr] = world.TryGetComponents
                    <Components::LocalTransform,
                    Components::CircleCollider2D>(B);

                //skip if transformation or collider components are missing from objs
                if (!tA_ptr || !cA_ptr || !tB_ptr || !cB_ptr) continue;

                // Skip disabled actives
                if (const auto* activeA = world.TryGet<Components::Active>(A)) {
                    if (!activeA->Enabled) continue;
                }
                if (const auto* activeB = world.TryGet<Components::Active>(B)) {
                    if (!activeB->Enabled) continue;
                }

                // If neither has a rigidbody/velocity, skip since no possible dyanmic response
                bool hasPhysicsA = world.TryGet<Components::Rigidbody2D>(A) && world.TryGet<Components::LinearVelocity2D>(A);
                bool hasPhysicsB = world.TryGet<Components::Rigidbody2D>(B) && world.TryGet<Components::LinearVelocity2D>(B);

                // static objs for both skip
                if (!hasPhysicsA && !hasPhysicsB) {
                    continue;
                }

                // get references to components 
                //transforms
                auto& tA = *tA_ptr;
                auto& tB = *tB_ptr;
                //collider components 
                const auto& cA = *cA_ptr;
                const auto& cB = *cB_ptr;

                // compute world-space centers
                const Vector3D centerA3 = tA.Position + Vector3D(cA.Offset.X, cA.Offset.Y, 0.0f);
                const Vector3D centerB3 = tB.Position + Vector3D(cB.Offset.X, cB.Offset.Y, 0.0f);
                const Vector2D centerA{ centerA3.X, centerA3.Y };
                const Vector2D centerB{ centerB3.X, centerB3.Y };

                const float rA = cA.Radius;
                const float rB = cB.Radius;

                // narrow phase testing is here (circle - circle) distance check
                // if circles overlap: distance^2 < (radiusA + radiusB)^2 -> means collision!
                const float dx = centerB.X - centerA.X;
                const float dy = centerB.Y - centerA.Y;
                const float distSq = dx * dx + dy * dy;
                const float radii = rA + rB;

                // reject if not collision
                if (distSq >= radii * radii) continue;
                // track the early exit
                collisionsResolved++;

                // prepare components for resolution - get or create default components for 
                // static entities -> 0 mass, 0 velocity
                Components::Rigidbody2D rbA{ 0 }, rbB{ 0 };
                Components::LinearVelocity2D velA{ {0,0} }, velB{ {0,0} };

                // try to get existing rigidbodies and velocities
                const auto* rbA_ptr = world.TryGet<Components::Rigidbody2D>(A);
                const auto* rbB_ptr = world.TryGet<Components::Rigidbody2D>(B);
                auto* velA_ptr = world.TryGet<Components::LinearVelocity2D>(A);
                auto* velB_ptr = world.TryGet<Components::LinearVelocity2D>(B);

                bool hasRbA = rbA_ptr != nullptr;
                bool hasRbB = rbB_ptr != nullptr;
                bool hasVelA = velA_ptr != nullptr;
                bool hasVelB = velB_ptr != nullptr;

                // copy existing components
                if (hasRbA) rbA = *rbA_ptr;
                if (hasRbB) rbB = *rbB_ptr;
                if (hasVelA) velA = *velA_ptr;
                if (hasVelB) velB = *velB_ptr;

                // get physics materials - friction, bounciness
                // default value set - friction, restituion, correction
                Components::PhysicsMaterial2D matA{ 0.2f, 0.5f, 0.5f }, matB{ 0.2f, 0.5f, 0.5f };
                if (const auto* matA_ptr = world.TryGet<Components::PhysicsMaterial2D>(A)) {
                    matA = *matA_ptr;
                }
                if (const auto* matB_ptr = world.TryGet<Components::PhysicsMaterial2D>(B)) {
                    matB = *matB_ptr;
                }

                //combine materials between objs
                //friction - averaged
                //restituion - higher value -> more bouncier is taken
                //position correction - averaged
                Components::PhysicsMaterial2D combinedMat{
                    (matA.Friction + matB.Friction) * 0.5f,
                    std::max(matA.Restitution, matB.Restitution),
                    (matA.PositionCorrectPercent + matB.PositionCorrectPercent) * 0.5f
                };

                float dist = std::sqrt(distSq);
                float depth = radii - dist;

                // Collision normal points from A to B
                Vector2D normal;
                if (dist > 0.0001f) {
                    normal.X = dx / dist;
                    normal.Y = dy / dist;
                }
                // Circles are perfectly overlapping, use arbitrary normal
                else {

                    normal.X = 1.0f;
                    normal.Y = 0.0f;
                }

                // resolved using our resolve collision
                // takes rigidbodies, velocity, transforms, offsets and combined materials
                Engine::Physics::ResolveCollision(
                    rbA, rbB,
                    velA, velB,
                    tA, tB,
                    normal,
                    depth,
                    combinedMat
                );

                // write back results
                if (hasVelA && velA_ptr) *velA_ptr = velA;
                if (hasVelB && velB_ptr) *velB_ptr = velB;
                // transforms are updated in-place by ref tA, tB
            } // for candidatePairs
            // early exits: if no collision happens in this iteration.
            if (collisionsResolved == 0) break;
        } // iterations

    } // namespace ECS

}