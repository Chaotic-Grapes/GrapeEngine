/**
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file PhysicsSystem.cpp
* @brief Broad/narrow-phase utilities and per-frame 2D physics update loop.
*
* @details
* This translation unit implements the main 2D physics system for the ECS.
* Responsibilities include:
* - Spatial hashing grid (broad phase) to prune collision checks
* - Shape tests (circle-circle, box-box, circle-box) composing narrow phase
* - Time integration for dynamic bodies (linear + angular)
* - Optional world-boundary constraint application
* - Iterative position correction and velocity resolution using Physics helpers
*
* The implementation favors clarity and robustness with early-outs and explicit
* checks. It relies on plain ECS components and engine physics helpers for
* reusable math and manifold building.
*
* @sources
* https://saeed1262.github.io/blog/2025/spatial-hashing-collision/
* break down of implementing of spatial hashing method
* linking it to broadphase collisions checking
* finishing off with quick speed narrow phase checking
* overall optimise collision checks to a low amount
* making this a systematic autonomous approach to collision
* systems
*
* @dependencies
* - ecs/systems/PhysicsSystem.h, services/Time.h
* - physics/Collision.h, physics/Physics.h, ecs/Components.h
* - helpers/MathUtils.h, helpers/EntityUtils.h
* - <unordered_map>, <unordered_set>, <vector>, <cmath>, <algorithm>, <iostream>
*/

#include "ecs/systems/PhysicsSystem.h"
#include "services/TimeSystem.h"
#include "physics/Collision.h"
#include "physics/Physics.h"
#include "physics/LayerMask.h"
#include "ecs/Components.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <algorithm>
#include "helpers/MathUtils.h"
#include "helpers/EntityUtils.h"
#include <iostream>
#include "audio/FmodAudioDevice.h"
#include "ecs/events/EventComponents.h"
#include "ecs/events/EventDispatcher.h"
#include "math/Vector3D.h"

extern Audio::FmodAudioDevice* gAudioDevice;


#ifndef PHYSICS_AUDIO_DEVICE
#define PHYSICS_AUDIO_DEVICE (gAudioDevice)
#endif

/**
* @brief: Class that divides active world into a grid to quickly find entities. Application of this
* in relation to the broad-narrow phase collision detection
*/
class SpatialPartitioning {
public:
    // cell sizes in float to determine the space size 
    // Tune this based on typical collider sizes / scene density.
    static constexpr float CELL_SIZE = 16.0f;

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
                m_grid[CellCoord{ cx, cy }].push_back(entity);
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
 * @brief: main physics update function
 * called every frame to:
 * 1. integrate velocities
 * 2. build spatial grid
 * 3. generate collision pairs
 * 4. resolve collisions
 */

namespace ECS {

    SystemMetadata PhysicsSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Physics");
        // Read accesses
        builder.ReadComponent<Components::LocalTransform>();
        builder.ReadComponent<Components::CircleCollider2D>();
        builder.ReadComponent<Components::BoxCollider2D>();
        builder.ReadComponent<Components::Rigidbody2D>();
        builder.ReadComponent<Components::Active>();
        // Write accesses
        builder.WriteComponent<Components::LocalTransform>();
        builder.WriteComponent<Components::Rigidbody2D>();
        // Execution order in Physics group
        builder.SetExecutionOrder(0);
        return builder.Build();
    }

    void PhysicsSystem::OnDestroy(World& world) {
        m_previousCollisions.clear();
    }

    // =====================================================================
    // Narrow-phase helpers
    // =====================================================================


    /**
    * @brief Circle-circle overlap test with normal/depth output.
    * @return true if overlapping; normal points A->B, depth is penetration.
    */
    bool TestCircleCircle(
        const Components::CircleCollider2D& circleA,
        const Components::LocalTransform& transformA,
        const Components::CircleCollider2D& circleB,
        const Components::LocalTransform& transformB,
        Vector2D& outNormal,
        float& outDepth)
    {
        // Compute world-space centers (apply local collider offsets).
        Vector2D centerA(
            transformA.Position.X + circleA.Offset.X,
            transformA.Position.Y + circleA.Offset.Y
        );
        Vector2D centerB(
            transformB.Position.X + circleB.Offset.X,
            transformB.Position.Y + circleB.Offset.Y
        );

        // Collision test
        // Delta and distance-squared.
        const float dx = centerB.X - centerA.X;
        const float dy = centerB.Y - centerA.Y;
        const float distSq = dx * dx + dy * dy;
        const float radiiSum = circleA.Radius + circleB.Radius;

        // Quick reject
        if (distSq >= radiiSum * radiiSum) {
            return false; // No collision
        }

        // Calculate normal and depth
        const float dist = std::sqrt(distSq);
        if (dist > 1e-6f) {
            outNormal = Vector2D(dx / dist, dy / dist);
        }
        else {
            // Arbitrary axis when centers coincide.
            outNormal = Vector2D(1.0f, 0.0f);
        }
        outDepth = radiiSum - dist;
        return true;
    }


    /**
     * @brief Test box-box collision using Collision utility
     */
    Engine::Collision::ContactManifold TestBoxBox(
        const Components::BoxCollider2D& boxA,
        const Components::LocalTransform& transformA,
        const Components::BoxCollider2D& boxB,
        const Components::LocalTransform& transformB)
    {
        // Convert to world-space centers
        Vector2D centerA(
            transformA.Position.X + boxA.Offset.X,
            transformA.Position.Y + boxA.Offset.Y
        );
        Vector2D centerB(
            transformB.Position.X + boxB.Offset.X,
            transformB.Position.Y + boxB.Offset.Y
        );

        // Do AABB test
        Engine::Collision::AABB aabbA = Engine::Collision::MakeAABBCenterSize(
            centerA, boxA.HalfExtents * 2.0f
        );
        Engine::Collision::AABB aabbB = Engine::Collision::MakeAABBCenterSize(
            centerB, boxB.HalfExtents * 2.0f
        );

        Vector2D normal;
        float penetration;

        // Check if boxes overlap
        if (!Engine::Collision::AABBvsAABB(aabbA, aabbB, &normal, &penetration)) {
            // No collision - return empty manifold
            return Engine::Collision::ContactManifold();
        }

        // create simple maniford
        Engine::Collision::ContactManifold manifold;
        manifold.normal = normal;
        manifold.penetration = penetration;

        // Single contact point at midpoint
        manifold.points[0].X = (centerA.X + centerB.X) * 0.5f;
        manifold.points[0].Y = (centerA.Y + centerB.Y) * 0.5f;
        manifold.pointCount = 1;

        return manifold;
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
        // World-space centers.
        Vector2D circleCenter(
            circleTransform.Position.X + circle.Offset.X,
            circleTransform.Position.Y + circle.Offset.Y
        );
        Vector2D boxCenter(
            boxTransform.Position.X + box.Offset.X,
            boxTransform.Position.Y + box.Offset.Y
        );

        // Build engine shapes for helper.
        Engine::Collision::AABB aabb = Engine::Collision::MakeAABBCenterSize(
            boxCenter, box.HalfExtents * 2.0f
        );
        Engine::Collision::Circle circ;
        circ.Center = circleCenter;
        circ.Radius = circle.Radius;

        // temp manifold to capture results
        Engine::Collision::Manifold manifold;
        if (Engine::Collision::Overlap(circ, aabb, &manifold)) {
            outNormal = manifold.Normal;
            outDepth = manifold.Penetration;
            return true;
        }

        return false;
    }

    // Helper to create unique collision pair ID
    static uint64_t MakeCollisionPairID(Entity a, Entity b) {
        uint32_t idA = a.Index;
        uint32_t idB = b.Index;
        // Ensure consistent ordering (smaller ID first)
        if (idA > idB) std::swap(idA, idB);
        return (static_cast<uint64_t>(idA) << 32) | idB;
    }


    // =====================================================================
    // PhysicsSystem::Update - main per-frame step
    // =====================================================================


    /**
    * @brief Integrate dynamics, build broad phase, test narrow phase, resolve.
    */
    void PhysicsSystem::OnUpdate(World& world, const float dt) {
        if (!Engine::Physics::IsEnabled()) return;
        if (dt <= 0.0f) return;

        //  Choose substep count; make it a tunable or cvar if you like.
        const int   substeps = 8;                         //higher = more stable, slower
        const float subDt = dt / static_cast<float>(substeps);

        // Create event dispatcher for firing collision events
        ECS::Events::EventDispatcher eventDispatcher(&world);

        // Clear any lingering event components from previous frames
        // (old code used a global CollisionEventQueue; now remove components directly)
        world.Each<ECS::Events::CollisionEvent>([&](Entity e, ECS::Events::CollisionEvent&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::CollisionEvent>(e))
                world.Remove<ECS::Events::CollisionEvent>(e);
        });
        world.Each<ECS::Events::TriggerEvent>([&](Entity e, ECS::Events::TriggerEvent&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::TriggerEvent>(e))
                world.Remove<ECS::Events::TriggerEvent>(e);
        });
        world.Each<ECS::Events::CollisionExitEvent>([&](Entity e, ECS::Events::CollisionExitEvent&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::CollisionExitEvent>(e))
                world.Remove<ECS::Events::CollisionExitEvent>(e);
        });
        world.Each<ECS::Events::TriggerExitEvent>([&](Entity e, ECS::Events::TriggerExitEvent&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::TriggerExitEvent>(e))
                world.Remove<ECS::Events::TriggerExitEvent>(e);
        });
        //to clock currentcollision pairs between A and B entities
        std::unordered_set<uint64_t> currentCollisions;
      

        // Running frame counter to reset per-frame SFX dedupe
        static uint64_t s_frameCounter = 0;
        ++s_frameCounter;


        //  Collect entity sets once per frame (usually fine).
        //  If your scene can add/remove colliders mid-frame, you can also
        //  move these two collectors *inside* the substep loop.
        std::vector<Entity> dynamicEntities;
        dynamicEntities.reserve(512);

        // Iterates all entities that have all four of those components (rigidbody, linear vel, angular vel, transform).
        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D,
            Components::AngularVelocity2D, Components::LocalTransform>(
                [&](const Entity e,
                    const Components::Rigidbody2D& rb,
                    Components::LinearVelocity2D&,
                    Components::AngularVelocity2D&,
                    Components::LocalTransform&) {
                        if (const auto* a = world.TryGet<Components::Active>(e)) if (!a->Enabled) return;
                        if (rb.Mass <= 0.0f) return; // only dynamics here
                        // Must have some collider to participate in broad-phase
                        if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) return;
                        dynamicEntities.push_back(e); //Push the entity into dynamicEntities for later broad-phase insertion
                });


        // Find and stores static entities
        std::vector<Entity> staticEntities;
        staticEntities.reserve(128);
        world.Each<Components::Rigidbody2D, Components::LocalTransform>(
            [&](const Entity e, const Components::Rigidbody2D& rb, const Components::LocalTransform&) {
                if (rb.Mass > 0.0f) return; // only statics here
                if (const auto* a = world.TryGet<Components::Active>(e)) if (!a->Enabled) return;
                if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) return;
                staticEntities.push_back(e); // Push to store and use static entities for checks later
            });

        // Substep loop: integrate and resolve in smaller time slices.
        for (int step = 0; step < substeps; ++step) {
            // Integrate dynamics with subDt and apply optional world bounds.
            world.Each<Components::Rigidbody2D, Components::LinearVelocity2D,
                Components::AngularVelocity2D, Components::LocalTransform>(
                    [&](const Entity e,
                        const Components::Rigidbody2D& rb,
                        Components::LinearVelocity2D& linVel,
                        Components::AngularVelocity2D& angVel,
                        Components::LocalTransform& xf)
                    {
                        if (const auto* a = world.TryGet<Components::Active>(e)) if (!a->Enabled) return;
                        if (rb.Mass <= 0.0f) return; // only dynamic bodies integrate

                        // Linear acceleration (forces, drag)
                        Vector2D acc = Engine::Physics::CalculateAcceleration(rb, linVel);
                        if (rb.Flags & (1 << 1)) { // gravity flag
                            acc += Engine::Physics::GetGravity() * rb.GravityScale;
                        }

                        // Semi-implicit Euler using subDt
                        linVel.Value.X += acc.X * subDt;
                        linVel.Value.Y += acc.Y * subDt;
                        xf.Position.X += linVel.Value.X * subDt;
                        xf.Position.Y += linVel.Value.Y * subDt;

                        // Angular if not fixed rotation
                        if (!(rb.Flags & (1 << 2))) {
                            const float angAcc = Engine::Physics::CalculateAngularAcceleration(rb, angVel);
                            if (std::abs(angAcc * subDt) > std::abs(angVel.Value)) angVel.Value = 0.0f;
                            else angVel.Value += angAcc * subDt;

                            xf.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angVel.Value * subDt) * xf.Rotation;
                        }

                        // Optional world boundary constraint
                        if (Engine::Physics::IsWorldBoundsEnabled()) {
                            float restitution = -1.0f;
                            if (const auto* m = world.TryGet<Components::PhysicsMaterial2D>(e)) restitution = m->Restitution;

                            Vector2D pos2D(xf.Position.X, xf.Position.Y);
                            Vector2D vel2D = linVel.Value;

                            //world bounds if circle collider
                            if (const auto* c = world.TryGet<Components::CircleCollider2D>(e)) {
                                if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, c->Radius,
                                    Engine::Physics::GetWorldBounds(),
                                    restitution)) {
                                    xf.Position.X = pos2D.X; xf.Position.Y = pos2D.Y; linVel.Value = vel2D;
                                }
                            }
                            //world bounds if box collider
                            else if (const auto* b = world.TryGet<Components::BoxCollider2D>(e)) {
                                const float approxR = std::max(b->HalfExtents.X, b->HalfExtents.Y);
                                if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, approxR,
                                    Engine::Physics::GetWorldBounds(),
                                    restitution)) {
                                    xf.Position.X = pos2D.X; xf.Position.Y = pos2D.Y; linVel.Value = vel2D;
                                }
                            }
                        }
                    });

            // Broad phase for this substep (rebuild grid because poses changed).
            SpatialPartitioning grid;
            auto insertEntity = [&](Entity e) {
                const auto* t = world.TryGet<Components::LocalTransform>(e);
                if (!t) return;
                if (const auto* c = world.TryGet<Components::CircleCollider2D>(e)) {
                    grid.Insert(e, t->Position, c->Radius);
                }
                else if (const auto* b = world.TryGet<Components::BoxCollider2D>(e)) {
                    grid.InsertBox(e, t->Position, b->HalfExtents);
                }
                };
            for (Entity e : dynamicEntities) insertEntity(e);
            for (Entity e : staticEntities)  insertEntity(e);

            // Candidate pairs deduped per substep.
            std::vector<std::pair<Entity, Entity>> pairs;
            std::unordered_set<uint64_t>           seen;
            pairs.reserve(dynamicEntities.size() * 4);
            seen.reserve(dynamicEntities.size() * 4);

            // Builds a list of unique candidate collision pairs from each spatial - grid cell, deduplicating pairs that appear in multiple cells.
            auto pairKey = [](uint64_t a, uint64_t b) -> uint64_t { if (a > b) std::swap(a, b); return (a << 32) | (b & 0xffffffffull); };

            // Iterates all occupied cells in the spatial hash/grid. Each cell has a small list of entities that overlap that cell.
            for (const auto& cell : grid.Grid()) {
                const auto& ents = cell.second;
                // Enumerate all unordered pairs within the cell by running i from 0..n-2 and j from i+1..n-1.
                for (size_t i = 0; i + 1 < ents.size(); ++i) {
                    for (size_t j = i + 1; j < ents.size(); ++j) {
                        //Packs the pair (ents[i], ents[j]) into the 64-bit canonical key using pairKey
                        const uint64_t key = pairKey(ECS::EntityUtils::Pack(ents[i]), ECS::EntityUtils::Pack(ents[j]));
                        if (seen.insert(key).second) pairs.emplace_back(ents[i], ents[j]);
                    }
                }
            }

            // Narrow phase + resolution for this substep.
            // You can reduce the inner iterative solver because substeps already help stability.

            const int solverIters = 4; // e.g., fewer than your original 8

            // run several small correction passes to improve stability.
            for (int it = 0; it < solverIters; ++it) {
                int resolved = 0;
                // Iterate all broad-phase candidate pairs (A,B).
                for (auto [A, B] : pairs) {
                    // Skip if either entity got destroyed during earlier steps.
                    if (!world.IsAlive(A) || !world.IsAlive(B)) continue;
                    // Fetch transforms; narrow phase needs world-space poses.
                    auto* tA = world.TryGet<Components::LocalTransform>(A);
                    auto* tB = world.TryGet<Components::LocalTransform>(B);
                    // cannot resolve without positions
                    if (!tA || !tB) continue;
                    // Honor Active flags: if present and disabled, skip.
                    if (const auto* aA = world.TryGet<Components::Active>(A); aA && !aA->Enabled) continue;
                    if (const auto* aB = world.TryGet<Components::Active>(B); aB && !aB->Enabled) continue;

                    // Query collider shapes present on each entity.
                    const auto* circA = world.TryGet<Components::CircleCollider2D>(A);
                    const auto* boxA = world.TryGet<Components::BoxCollider2D>(A);
                    const auto* circB = world.TryGet<Components::CircleCollider2D>(B);
                    const auto* boxB = world.TryGet<Components::BoxCollider2D>(B);
                    // If either side has no collider, this pair cannot collide.
                    if ((!circA && !boxA) || (!circB && !boxB)) continue;

                    // --- Layer mask filtering ---
                    // Determine each entity's layer id (default to 0) and the collider's mask
                    uint16_t layerAId = 0u;
                    uint16_t layerBId = 0u;
                    if (const auto* la = world.TryGet<Components::Layer>(A))
                        layerAId = la->Id;
                    if (const auto* lb = world.TryGet<Components::Layer>(B))
                        layerBId = lb->Id;

                    uint32_t maskA = 0xFFFFFFFFu;
                    uint32_t maskB = 0xFFFFFFFFu;
                    if (circA)
                        maskA = circA->LayerMask;
                    else if (boxA)
                        maskA = boxA->LayerMask;
                    if (circB)
                        maskB = circB->LayerMask;
                    else if (boxB)
                        maskB = boxB->LayerMask;

                    // If masks/layers indicate no collision, skip early.
                    if (!Engine::CanCollide(maskA, layerAId, maskB, layerBId))
                        continue;

                    // Require at least one side to be physically simulated (has rb + velocity).
                    const bool hasPhysA = world.TryGet<Components::Rigidbody2D>(A) && world.TryGet<Components::LinearVelocity2D>(A);
                    const bool hasPhysB = world.TryGet<Components::Rigidbody2D>(B) && world.TryGet<Components::LinearVelocity2D>(B);
                    if (!hasPhysA && !hasPhysB) continue;

                    // Narrow phase: run the appropriate shape test to get contact normal and depth.
                    Engine::Collision::ContactManifold manifold;
                    bool hasCollision = false;

                    if (circA && circB) {
                        // Circle-circle: single contact point (keep old method for now)
                        Vector2D n;
                        float depth;
                        if (TestCircleCircle(*circA, *tA, *circB, *tB, n, depth)) {
                            manifold.normal = n;
                            manifold.penetration = depth;
                            // Calculate contact point (between centers)
                            manifold.points[0] = Vector2D(
                                (tA->Position.X + tB->Position.X) * 0.5f,
                                (tA->Position.Y + tB->Position.Y) * 0.5f
                            );
                            manifold.pointCount = 1;
                            hasCollision = true;
                        }
                    }
                    else if (boxA && boxB) {
                        // Box-box: use new manifold generation
                        manifold = TestBoxBox(*boxA, *tA, *boxB, *tB);
                        hasCollision = (manifold.pointCount > 0);
                    }
                    else if (circA && boxB) {
                        // Circle-box: single contact point
                        Vector2D n;
                        float depth;
                        if (TestCircleBox(*circA, *tA, *boxB, *tB, n, depth)) {
                            manifold.normal = n;
                            manifold.penetration = depth;
                            manifold.points[0] = Vector2D(
                                tA->Position.X + circA->Offset.X,
                                tA->Position.Y + circA->Offset.Y
                            );
                            manifold.pointCount = 1;
                            hasCollision = true;
                        }
                    }
                    else if (boxA && circB) {
                        // Box-circle: single contact point
                        Vector2D n;
                        float depth;
                        if (TestCircleBox(*circB, *tB, *boxA, *tA, n, depth)) {
                            manifold.normal = -n;  // Flip normal
                            manifold.penetration = depth;
                            manifold.points[0] = Vector2D(
                                tB->Position.X + circB->Offset.X,
                                tB->Position.Y + circB->Offset.Y
                            );
                            manifold.pointCount = 1;
                            hasCollision = true;
                        }
                    }

                    if (!hasCollision) continue;

                    uint64_t pairID = MakeCollisionPairID(A, B);
                    currentCollisions.insert(pairID);

                    bool isNewCollision = (m_previousCollisions.find(pairID) == m_previousCollisions.end());

                    // Fire collision events using EventDispatcher
                    if (isNewCollision) {
                        // New collision detected
                        eventDispatcher.FireCollisionEvent(
                            A.Index, B.Index,
                            Vector3D(manifold.points[0].X, manifold.points[0].Y, 0.0f),
                            Vector3D(manifold.normal.X, manifold.normal.Y, 0.0f),
                            Vector3D(0.0f, 0.0f, 0.0f),  // TODO: Compute relative velocity
                            0.0f  // TODO: Compute impact magnitude
                        );
                        eventDispatcher.FireCollisionEvent(
                            B.Index, A.Index,
                            Vector3D(manifold.points[0].X, manifold.points[0].Y, 0.0f),
                            Vector3D(-manifold.normal.X, -manifold.normal.Y, 0.0f),
                            Vector3D(0.0f, 0.0f, 0.0f),  // TODO: Compute relative velocity
                            0.0f  // TODO: Compute impact magnitude
                        );
                    }

                    // Gather physics state (by value) and current velocities; some may be missing.
                    Components::Rigidbody2D      rbA{ 0 }, rbB{ 0 };
                    Components::LinearVelocity2D vA{ {0,0} }, vB{ {0,0} };

                    // Read component pointers; if present, copy their values into locals.
                    const auto* rbAp = world.TryGet<Components::Rigidbody2D>(A);
                    const auto* rbBp = world.TryGet<Components::Rigidbody2D>(B);
                    auto* vAp = world.TryGet<Components::LinearVelocity2D>(A);
                    auto* vBp = world.TryGet<Components::LinearVelocity2D>(B);
                    if (rbAp) rbA = *rbAp; if (rbBp) rbB = *rbBp;
                    if (vAp)  vA = *vAp;  if (vBp)  vB = *vBp;

                    // Fetch materials (friction, restitution, position-correction factor).
                    // Use sensible defaults if an entity has no material component.
                    Components::PhysicsMaterial2D mA{ 0.2f,0.5f,0.5f }, mB{ 0.2f,0.5f,0.5f };
                    if (const auto* mpA = world.TryGet<Components::PhysicsMaterial2D>(A)) mA = *mpA;
                    if (const auto* mpB = world.TryGet<Components::PhysicsMaterial2D>(B)) mB = *mpB;

                    // Combine materials for the contact:
                    // - friction: average (common simple heuristic)
                    // - restitution: take the bouncier of the two
                    // - position-correct percent: average
                    const Components::PhysicsMaterial2D mCombined{
                        (mA.Friction + mB.Friction) * 0.5f,
                        std::max(mA.Restitution, mB.Restitution),
                        (mA.PositionCorrectPercent + mB.PositionCorrectPercent) * 0.5f
                    };

                    // Resolve
                    Engine::Physics::ResolveCollisionManifold(rbA, rbB, vA, vB, *tA, *tB, manifold, mCombined);

                    // Write back
                    if (vAp) *vAp = vA;
                    if (vBp) *vBp = vB;

                    ++resolved;
                    {
                        // Compute impact magnitude from relative velocity along normal BEFORE the next iteration changes it further.
                        const Vector2D rel = vB.Value - vA.Value;
                        const float vn = rel.X * manifold.normal.X + rel.Y * manifold.normal.Y;
                        const float    impactSpeed = std::abs(vn);

                        // Filter tiny contacts to avoid spam; tune as needed.
                        constexpr float kImpactThreshold = 80.0f;

                        if (impactSpeed >= kImpactThreshold) {
                            // Per-frame dedupe for this pair
                            static uint64_t s_lastFrameSeen = 0;
                            static std::unordered_set<uint64_t> sfxPlayedThisFrame;
                            if (s_lastFrameSeen != s_frameCounter) {
                                sfxPlayedThisFrame.clear(); s_lastFrameSeen = s_frameCounter;
                            }
                            const uint64_t pk = pairKey(ECS::EntityUtils::Pack(A), ECS::EntityUtils::Pack(B));
                            if (sfxPlayedThisFrame.insert(pk).second) {
                                if (auto* audio = PHYSICS_AUDIO_DEVICE) {
                                    const std::string cue = "sfx_collide";
                                    const std::string path =
                                        std::filesystem::absolute("assets/Audio/SFX/Squishy-Splatter_1.wav").string();
                                    Audio::SoundParams sp; sp.Stream = false; sp.Is3D = false;
                                    // Preload is cheap after first time; keep for safety:
                                    audio->LoadCue(cue, path, sp);

                                    Audio::PlaySettings ps;
                                    ps.Loop = false;
                                    // Scale volume by impact; clamp to [0.2, 1.0]
                                    ps.Volume = std::max(0.2f, std::min(impactSpeed / 350.0f, 1.0f));
                                    ps.Pitch = 1.0f;
                                    audio->PlaySingle(cue, ps, Audio::PlayPolicy::SingleInstanceRestart);
                                }
                            }
                        }
                    }
                }

                if (resolved == 0) break;
            }

            for (uint64_t pairID : m_previousCollisions) {
                if (currentCollisions.find(pairID) == currentCollisions.end()) {
                    // Collision ended
                    uint32_t idA = (pairID >> 32);
                    uint32_t idB = (pairID & 0xFFFFFFFF);

                    Entity entityA{ idA, 0 };
                    Entity entityB{ idB, 0 };

                    if (world.IsAlive(entityA) && world.IsAlive(entityB)) {
                        // Fire collision exit events using EventDispatcher
                        eventDispatcher.FireCollisionExitEvent(
                            idA, idB,
                            Vector3D(0.0f, 0.0f, 0.0f)  // TODO: Track last contact point
                        );
                        eventDispatcher.FireCollisionExitEvent(
                            idB, idA,
                            Vector3D(0.0f, 0.0f, 0.0f)  // TODO: Track last contact point
                        );
                    }
                }
            }

            m_previousCollisions = std::move(currentCollisions);

            // (Optional) Integrate positions/orientations here if you separate velocity & pose updates.
        }

        // Clear ephemeral event components at end of frame
        eventDispatcher.ClearFrameEvents();
    }
}// namespace ECS