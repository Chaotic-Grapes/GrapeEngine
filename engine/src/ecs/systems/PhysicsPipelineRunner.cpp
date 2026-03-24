/* Start Header *****************************************************************/
/*!
\file   PhysicsPipelineRunner.cpp
\author Dalton Koh Shi Hao (100%)
\par    d.koh@digipen.edu

\brief
Internal staged 2D physics pipeline implementation.

\details
This translation unit hosts the parity-first physics frame pipeline extracted
from PhysicsSystem::OnUpdate. It handles:
- per-frame gather and cache
- broad-phase and narrow-phase collision
- iterative solver and tilemap resolution
- collision/trigger event finalization

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include "ecs/systems/internal/PhysicsPipelineRunner.h"
#include "ecs/systems/PhysicsSystem.h"
#include "physics/Collision.h"
#include "physics/Physics.h"
#include "physics/LayerMask.h"
#include "scene/LayerManager.h"
#include "ecs/Components.h"
#include "core/World/TileMap.hpp"
#include "core/World/TileTypes.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include "helpers/EntityUtils.h"
#include "helpers/TransformUtils.h"
#include "core/Logger.h"
#include "ecs/events/EventDispatcher.h"
#include "math/Vector3D.h"

// Divides active world space into fixed cells so broad-phase can query nearby entities efficiently
class SpatialPartitioning {
public:

    // Cell size controls broad-phase granularity and should reflect typical collider footprints
    // Larger cells reduce grid bookkeeping while smaller cells reduce candidate pair fanout
    static constexpr float CELL_SIZE = 16.0f;

    // Stores integer grid coordinates for one spatial cell
    struct CellCoord {
        int x, y;

        // Compares cell coordinates so map lookups can match exact cell keys
        bool operator==(const CellCoord& other) const { return x == other.x && y == other.y; }
    };

    // Hashes cell coordinates for unordered_map key usage
    struct CellHash {
        size_t operator()(const CellCoord& c) const noexcept {

            // Mix x and y so adjacent cells spread across buckets instead of clustering by one axis
            return std::hash<int>{}(c.x) ^ (std::hash<int>{}(c.y) << 1);
        }
    };

    // Inserts a radius-based collider by mapping its AABB bounds into all overlapped grid cells
    void Insert(const ECS::Entity entity, const Vector3D& position, float radius) {

        // floor converts coordinate to containing cell index so negative positions still map correctly
        // Compute min and max cell coverage along x from radius-expanded center
        int minX = static_cast<int>(std::floor((position.X - radius) / CELL_SIZE));
        int maxX = static_cast<int>(std::floor((position.X + radius) / CELL_SIZE));

        // Compute min and max cell coverage along y from radius-expanded center
        int minY = static_cast<int>(std::floor((position.Y - radius) / CELL_SIZE));
        int maxY = static_cast<int>(std::floor((position.Y + radius) / CELL_SIZE));

        // Insert entity into each covered cell so nearby queries can find this collider quickly
        for (int cx = minX; cx <= maxX; ++cx) {
            for (int cy = minY; cy <= maxY; ++cy) {
                m_grid[CellCoord{ cx, cy }].push_back(entity);
            }
        }
    }

    // Inserts a box collider by mapping its AABB extents into all overlapped grid cells
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

    // Returns mutable access to grid for broad-phase pair enumeration
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash>& Grid() { return m_grid; }

private:

    // Maps each occupied cell to the entities whose broad-phase bounds overlap that cell
    std::unordered_map<CellCoord, std::vector<ECS::Entity>, CellHash> m_grid;
};

namespace {

    // Collision bitmask definitions for tile corners (2x2 subcells)
    constexpr uint8_t kCollisionMaskBottomLeft = 1 << 0;  // Bit 0: Bottom-left corner
    constexpr uint8_t kCollisionMaskBottomRight = 1 << 1; // Bit 1: Bottom-right corner
    constexpr uint8_t kCollisionMaskTopLeft = 1 << 2;     // Bit 2: Top-left corner
    constexpr uint8_t kCollisionMaskTopRight = 1 << 3;    // Bit 3: Top-right corner

}

// Builds effective world transform from local hierarchy so physics uses current parent chain state
// Avoids stale WorldTransform snapshots during substeps where transforms are being modified
static bool GetPhysicsWorldTransform(ECS::World& world, const ECS::Entity entity, ECS::Components::LocalTransform& outTransform) {
    const auto* localTransform = world.TryGet<ECS::Components::LocalTransform>(entity);
    if (!localTransform) {

        // No local transform means this entity isn't positioned relative to a parent,
        // so fall back to whatever world transform it has directly
        if (!world.Has<ECS::Components::WorldTransform>(entity)) {

            // Entity has neither local nor world transform so reconstruction cannot continue
            return false;
        }

        // Decompose the world matrix directly since there's no local hierarchy to walk
        const auto& worldTransform = world.Get<ECS::Components::WorldTransform>(entity);
        TransformUtils::DecomposeTRS(worldTransform.Matrix, outTransform.Position, outTransform.Rotation, outTransform.Scale);
        return true;
    }

    // Start with this entity's own local TRS as the base of the accumulated world matrix
    Matrix4x4 worldMatrix = TransformUtils::MakeTRS(localTransform->Position, localTransform->Rotation, localTransform->Scale);
    ECS::Entity parent = world.ParentOf(entity);

    while (!parent.IsNull() && world.IsAlive(parent)) {
        if (const auto* parentLocal = world.TryGet<ECS::Components::LocalTransform>(parent)) {

            // Parent has a local transform, so keep walking up; pre-multiply to accumulate
            // parent-space contribution correctly (parent TRS applied before child's)
            const Matrix4x4 parentMatrix = TransformUtils::MakeTRS(parentLocal->Position, parentLocal->Rotation, parentLocal->Scale);
            worldMatrix = parentMatrix * worldMatrix;
        }
        else if (world.Has<ECS::Components::WorldTransform>(parent)) {

            // Parent already has a baked world matrix; treat it as a root anchor and stop;
            // no point walking further since everything above is already in world space
            const auto& parentWorld = world.Get<ECS::Components::WorldTransform>(parent);
            worldMatrix = parentWorld.Matrix * worldMatrix;
            break;
        }
        else {

            // Parent has no transform data at all; hierarchy is broken here, stop walking
            break;
        }

        parent = world.ParentOf(parent);
    }

    // Pull position, rotation, scale back out of the final accumulated matrix for the caller
    TransformUtils::DecomposeTRS(worldMatrix, outTransform.Position, outTransform.Rotation, outTransform.Scale);
    return true;
}

namespace ECS {
    // =====================================================================
    // Narrow-phase helpers
    // =====================================================================

    // Returns 2D dot product used in SAT projection and basis transformation math
    static float Dot2D(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    // Tests world-space circle-circle overlap and outputs normal from A to B plus penetration depth
    bool TestCircleCircle(
        const Engine::WorldCircle& circleA,
        const Engine::WorldCircle& circleB,
        Vector2D& outNormal,
        float& outDepth)
    {

        // Collision test using pre-computed world-space circles
        const float dx = circleB.Center.X - circleA.Center.X;
        const float dy = circleB.Center.Y - circleA.Center.Y;
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

            // Arbitrary axis when centers coincide
            outNormal = Vector2D(1.0f, 0.0f);
        }
        outDepth = radiiSum - dist;
        return true;
    }

    // Deprecated overload that converts local transform circle inputs into world circles then delegates
    bool TestCircleCircle(
        const Components::CircleCollider2D& circleA,
        const Components::LocalTransform& transformA,
        const Components::CircleCollider2D& circleB,
        const Components::LocalTransform& transformB,
        Vector2D& outNormal,
        float& outDepth)
    {

        // Compute world-space circles and delegate to the world-space version
        Engine::WorldCircle wcA = Engine::Physics::GetWorldCircle(circleA, transformA);
        Engine::WorldCircle wcB = Engine::Physics::GetWorldCircle(circleB, transformB);
        return TestCircleCircle(wcA, wcB, outNormal, outDepth);
    }

    // Tests world-space OBB overlap with SAT and returns manifold with normal penetration and contact point
    Engine::Collision::ContactManifold TestBoxBox(
        const Engine::WorldOBB& obbA,
        const Engine::WorldOBB& obbB)
    {

        // Collision test using Separating Axis Theorem (SAT) for OBBs
        Engine::Collision::ContactManifold manifold;
        manifold.pointCount = 0;

        // Compute difference vector between box centers
        const Vector2D delta = obbB.Center - obbA.Center;
        const Vector2D axes[4] = { obbA.AxisX, obbA.AxisY, obbB.AxisX, obbB.AxisY };

        // Track minimum overlap
        float minOverlap = FLT_MAX;
        Vector2D bestAxis{ 0.0f, 0.0f };

        // Iterate over all axes
        // The four axes to test are the normals of the edges of both boxes
        for (const auto& axis : axes) {

            // Project both boxes onto the axis
            const float rA =
                obbA.HalfExtents.X * std::abs(Dot2D(axis, obbA.AxisX)) +
                obbA.HalfExtents.Y * std::abs(Dot2D(axis, obbA.AxisY));
            const float rB =
                obbB.HalfExtents.X * std::abs(Dot2D(axis, obbB.AxisX)) +
                obbB.HalfExtents.Y * std::abs(Dot2D(axis, obbB.AxisY));
            const float dist = std::abs(Dot2D(delta, axis));
            const float overlap = rA + rB - dist;

            if (overlap <= 0.0f) {
                return manifold; // separating axis found
            }

            // Check for minimum overlap
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = axis;

                // Ensure normal points from A to B
                if (Dot2D(delta, axis) < 0.0f) {
                    bestAxis = bestAxis * -1.0f;
                }
            }
        }

        // Set manifold normal and penetration depth
        manifold.normal = bestAxis;
        manifold.penetration = minOverlap;

        // Compute contact point (approximate as midpoint of support points)
        // Support points are the furthest points on each box in the direction of the collision normal
        auto supportPoint = [](const Engine::WorldOBB& obb, const Vector2D& direction) {
            const float signX = (Dot2D(direction, obb.AxisX) >= 0.0f) ? 1.0f : -1.0f;
            const float signY = (Dot2D(direction, obb.AxisY) >= 0.0f) ? 1.0f : -1.0f;
            return obb.Center +
                obb.AxisX * (obb.HalfExtents.X * signX) +
                obb.AxisY * (obb.HalfExtents.Y * signY);
        };

        // Single contact point for box-box
        const Vector2D pA = supportPoint(obbA, manifold.normal);
        const Vector2D pB = supportPoint(obbB, manifold.normal * -1.0f);
        manifold.points[0] = (pA + pB) * 0.5f;
        manifold.pointCount = 1;

        return manifold;
    }

    // Deprecated overload that builds world OBB values from component plus transform inputs
    [[deprecated("Use the world-space OBB version instead")]]
    Engine::Collision::ContactManifold TestBoxBox(
        const Components::BoxCollider2D& boxA,
        const Components::LocalTransform& transformA,
        const Components::BoxCollider2D& boxB,
        const Components::LocalTransform& transformB)
    {

        // Compute world-space OBBs and delegate to the world-space version
        const Engine::WorldOBB obbA = Engine::Physics::GetWorldOBB(boxA, transformA);
        const Engine::WorldOBB obbB = Engine::Physics::GetWorldOBB(boxB, transformB);
        return TestBoxBox(obbA, obbB);
    }

    // Tests world-space circle-OBB overlap with axis-aligned fast path and local-space fallback path
    bool TestCircleBox(
        const Engine::WorldCircle& circle,
        const Engine::WorldOBB& box,
        Vector2D& outNormal,
        float& outDepth,
        Vector2D& outContact)
    {

        // Fast path for axis-aligned boxes
        if (std::abs(box.Rotation) < 1e-6f) {
            Engine::Collision::AABB aabb = Engine::Collision::MakeAABBCenterSize(
                box.Center, box.HalfExtents * 2.0f
            );

            // Create collision circle for fast path
            Engine::Collision::Circle circ;
            circ.Center = circle.Center;
            circ.Radius = circle.Radius;

            // Use AABB-circle overlap test
            Engine::Collision::Manifold manifold;
            if (Engine::Collision::Overlap(circ, aabb, &manifold)) {
                outNormal = manifold.Normal;
                outDepth = manifold.Penetration;
                outContact = manifold.Contact;
                return true;
            }

            return false;
        }

        // Transform circle center into box-local space
        const Vector2D toCircle = circle.Center - box.Center;
        const Vector2D localCenter{
            Dot2D(toCircle, box.AxisX),
            Dot2D(toCircle, box.AxisY)
        };

        Engine::Collision::Circle localCircle;
        localCircle.Center = localCenter;
        localCircle.Radius = circle.Radius;

        // Create box AABB in local space
        Engine::Collision::AABB localBox = Engine::Collision::MakeAABBCenterSize(
            Vector2D{ 0.0f, 0.0f },
            Vector2D{ box.HalfExtents.X * 2.0f, box.HalfExtents.Y * 2.0f }
        );

        // Perform local-space circle-AABB overlap test
        Engine::Collision::Manifold localManifold;
        if (!Engine::Collision::Overlap(localCircle, localBox, &localManifold)) {
            return false;
        }

        // Transform results back to world space
        outNormal = box.AxisX * localManifold.Normal.X + box.AxisY * localManifold.Normal.Y;
        outDepth = localManifold.Penetration;
        outContact = box.Center +
            box.AxisX * localManifold.Contact.X +
            box.AxisY * localManifold.Contact.Y;
        return true;
    }

    // Deprecated overload that converts local component inputs into world-space circle and OBB
    [[deprecated("Use the WorldCircle/WorldAABB version instead")]]
    bool TestCircleBox(
        const Components::CircleCollider2D& circle,
        const Components::LocalTransform& circleTransform,
        const Components::BoxCollider2D& box,
        const Components::LocalTransform& boxTransform,
        Vector2D& outNormal,
        float& outDepth,
        Vector2D& outContact)
    {

        // Compute world-space shapes and delegate to the world-space version
        Engine::WorldCircle wc = Engine::Physics::GetWorldCircle(circle, circleTransform);
        Engine::WorldOBB obb = Engine::Physics::GetWorldOBB(box, boxTransform);
        return TestCircleBox(wc, obb, outNormal, outDepth, outContact);
    }

    // Helper to create unique collision pair key (order-independent)
    static PackedEntityPair MakeCollisionPair(PackedEntityId a, PackedEntityId b) {
        if (a > b) std::swap(a, b);
        return PackedEntityPair{ a, b };
    }

    // Helper to create trigger pair key (order-dependent)
    static PackedEntityPair MakeTriggerPair(PackedEntityId triggerId, PackedEntityId otherId) {
        return PackedEntityPair{ triggerId, otherId };
    }

    class PhysicsPipelineRunner {
    public:
        PhysicsPipelineRunner(PhysicsSystem& systemRef, World& worldRef, Scenes::LayerManager& layerMgr, float frameDt)
            : system(systemRef), world(worldRef), layerManager(layerMgr), dt(frameDt), eventDispatcher(&worldRef) {}

        void Run() {
            system.RefreshRuntimeTileMaps(world);
            GatherEntities();

            constexpr int kSubsteps = 8;
            constexpr int kSolverIterations = 4;
            const float subDt = dt / static_cast<float>(kSubsteps);

            for (int step = 0; step < kSubsteps; ++step) {
                IntegrateDynamicBodies(subDt);
                BuildBodyCache();
                BuildBroadphasePairs();
                SolvePairs(kSolverIterations);
                ResolveTilemapCollisions();
            }

            FinalizeEvents();
        }

    private:
        struct BodyCache {
            Entity entity{};
            PackedEntityId packed{};
            bool isDynamic = false;
            bool hasPhysicsPair = false;
            bool isTrigger = false;
            uint16_t layerId = 0;
            uint32_t layerMask = 0xFFFFFFFFu;
            Components::LocalTransform* local = nullptr;
            Components::Rigidbody2D* rb = nullptr;
            Components::LinearVelocity2D* vel = nullptr;
            Components::AngularVelocity2D* angVel = nullptr;
            const Components::CircleCollider2D* circle = nullptr;
            const Components::BoxCollider2D* box = nullptr;
            Components::PhysicsMaterial2D material{ 0.2f, 0.5f, 0.5f };
            Components::LocalTransform worldTransform{};
            Engine::WorldCircle worldCircle{};
            Engine::WorldOBB worldObb{};
            Engine::WorldAABB worldAabb{};
            bool hasWorldShape = false;
        };

        static Components::PhysicsMaterial2D CombineMaterials(
            const Components::PhysicsMaterial2D& a,
            const Components::PhysicsMaterial2D& b)
        {
            return Components::PhysicsMaterial2D{
                (a.Friction + b.Friction) * 0.5f,
                std::max(a.Restitution, b.Restitution),
                (a.PositionCorrectPercent + b.PositionCorrectPercent) * 0.5f
            };
        }

        static bool BuildContactManifold(const BodyCache& a, const BodyCache& b, Engine::Collision::ContactManifold& out) {
            out = Engine::Collision::ContactManifold{};

            if (a.circle && b.circle) {
                Vector2D n{};
                float depth = 0.0f;
                if (!TestCircleCircle(a.worldCircle, b.worldCircle, n, depth)) {
                    return false;
                }
                out.normal = n;
                out.penetration = depth;
                const Vector2D pA = a.worldCircle.Center + (n * a.worldCircle.Radius);
                const Vector2D pB = b.worldCircle.Center - (n * b.worldCircle.Radius);
                out.points[0] = (pA + pB) * 0.5f;
                out.pointCount = 1;
                return true;
            }

            if (a.box && b.box) {
                out = TestBoxBox(a.worldObb, b.worldObb);
                return out.pointCount > 0;
            }

            if (a.circle && b.box) {
                Vector2D n{};
                float depth = 0.0f;
                Vector2D contact{};
                if (!TestCircleBox(a.worldCircle, b.worldObb, n, depth, contact)) {
                    return false;
                }
                out.normal = n;
                out.penetration = depth;
                out.points[0] = contact;
                out.pointCount = 1;
                return true;
            }

            if (a.box && b.circle) {
                Vector2D n{};
                float depth = 0.0f;
                Vector2D contact{};
                if (!TestCircleBox(b.worldCircle, a.worldObb, n, depth, contact)) {
                    return false;
                }
                out.normal = -n;
                out.penetration = depth;
                out.points[0] = contact;
                out.pointCount = 1;
                return true;
            }

            return false;
        }

        bool RefreshBodyWorldShape(BodyCache& body) const {
            if (!body.local || (!body.circle && !body.box)) {
                body.hasWorldShape = false;
                return false;
            }

            if (!GetPhysicsWorldTransform(world, body.entity, body.worldTransform)) {
                body.hasWorldShape = false;
                return false;
            }

            if (body.circle) {
                body.worldCircle = Engine::Physics::GetWorldCircle(*body.circle, body.worldTransform);
                body.worldAabb.Center = body.worldCircle.Center;
                body.worldAabb.HalfExtents = Vector2D(body.worldCircle.Radius, body.worldCircle.Radius);
            }
            else {
                body.worldObb = Engine::Physics::GetWorldOBB(*body.box, body.worldTransform);
                body.worldAabb = Engine::Physics::GetWorldAABB(*body.box, body.worldTransform);
            }

            body.hasWorldShape = true;
            return true;
        }

        void GatherEntities() {
            dynamicEntities.clear();
            staticEntities.clear();
            dynamicEntities.reserve(512);
            staticEntities.reserve(256);

            world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::LocalTransform>(
                [this](const Entity e, const Components::Rigidbody2D& rb, Components::LinearVelocity2D&, Components::LocalTransform&) {
                    if (!world.IsActiveInHierarchy(e) || rb.Mass <= 0.0f) {
                        return;
                    }
                    const auto* layer = world.TryGet<Components::Layer>(e);
                    const uint16_t layerId = layer ? layer->Id : 0;
                    if (!layerManager.Get(layerId).physicsEnabled) {
                        return;
                    }
                    if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) {
                        return;
                    }
                    dynamicEntities.push_back(e);
                });

            world.Each<Components::Rigidbody2D, Components::LocalTransform>(
                [this](const Entity e, const Components::Rigidbody2D& rb, const Components::LocalTransform&) {
                    if (!world.IsActiveInHierarchy(e) || rb.Mass > 0.0f) {
                        return;
                    }
                    const auto* layer = world.TryGet<Components::Layer>(e);
                    const uint16_t layerId = layer ? layer->Id : 0;
                    if (!layerManager.Get(layerId).physicsEnabled) {
                        return;
                    }
                    if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) {
                        return;
                    }
                    staticEntities.push_back(e);
                });

            std::unordered_set<EntityId> collectedIds;
            collectedIds.reserve(dynamicEntities.size() + staticEntities.size());
            for (const Entity e : dynamicEntities) {
                collectedIds.insert(e.Index);
            }
            for (const Entity e : staticEntities) {
                collectedIds.insert(e.Index);
            }

            world.Each<Components::LocalTransform>([this, &collectedIds](const Entity e, const Components::LocalTransform&) {
                if (collectedIds.contains(e.Index)) {
                    return;
                }
                if (!world.IsActiveInHierarchy(e) || world.Has<Components::Rigidbody2D>(e)) {
                    return;
                }
                if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) {
                    return;
                }
                const auto* layer = world.TryGet<Components::Layer>(e);
                const uint16_t layerId = layer ? layer->Id : 0;
                if (!layerManager.Get(layerId).physicsEnabled) {
                    return;
                }
                staticEntities.push_back(e);
                collectedIds.insert(e.Index);
            });
        }

        void IntegrateDynamicBodies(float subDt) {
            for (const Entity e : dynamicEntities) {
                if (!world.IsAlive(e) || !world.IsActiveInHierarchy(e)) {
                    continue;
                }

                auto* rb = world.TryGet<Components::Rigidbody2D>(e);
                auto* vel = world.TryGet<Components::LinearVelocity2D>(e);
                auto* xf = world.TryGet<Components::LocalTransform>(e);
                if (!rb || !vel || !xf || rb->Mass <= 0.0f) {
                    continue;
                }

                Vector2D acc = Engine::Physics::CalculateAcceleration(*rb, *vel);
                if (rb->Flags & (1u << 1)) {
                    acc += Engine::Physics::GetGravity() * rb->GravityScale;
                }

                vel->Value.X += acc.X * subDt;
                vel->Value.Y += acc.Y * subDt;
                xf->Position.X += vel->Value.X * subDt;
                xf->Position.Y += vel->Value.Y * subDt;

                if (!(rb->Flags & (1u << 2))) {
                    if (auto* angVel = world.TryGet<Components::AngularVelocity2D>(e)) {
                        const float angAcc = Engine::Physics::CalculateAngularAcceleration(*rb, *angVel);
                        if (std::abs(angAcc * subDt) > std::abs(angVel->Value)) {
                            angVel->Value = 0.0f;
                        }
                        else {
                            angVel->Value += angAcc * subDt;
                        }
                        xf->Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angVel->Value * subDt) * xf->Rotation;
                    }
                }

                if (!Engine::Physics::IsWorldBoundsEnabled()) {
                    continue;
                }

                float restitution = -1.0f;
                if (const auto* m = world.TryGet<Components::PhysicsMaterial2D>(e)) {
                    restitution = m->Restitution;
                }

                Vector2D pos2D(xf->Position.X, xf->Position.Y);
                Vector2D vel2D = vel->Value;

                if (const auto* c = world.TryGet<Components::CircleCollider2D>(e)) {
                    if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, c->Radius, Engine::Physics::GetWorldBounds(), restitution)) {
                        xf->Position.X = pos2D.X;
                        xf->Position.Y = pos2D.Y;
                        vel->Value = vel2D;
                    }
                }
                else if (const auto* b = world.TryGet<Components::BoxCollider2D>(e)) {
                    const float approxR = std::max(b->HalfExtents.X, b->HalfExtents.Y);
                    if (Engine::Physics::ApplyBoundaryConstraint(pos2D, vel2D, approxR, Engine::Physics::GetWorldBounds(), restitution)) {
                        xf->Position.X = pos2D.X;
                        xf->Position.Y = pos2D.Y;
                        vel->Value = vel2D;
                    }
                }
            }
        }

        void BuildBodyCache() {
            bodies.clear();
            bodyByPacked.clear();
            dynamicPacked.clear();
            bodies.reserve(dynamicEntities.size() + staticEntities.size());
            bodyByPacked.reserve(dynamicEntities.size() + staticEntities.size());
            dynamicPacked.reserve(dynamicEntities.size());

            auto addBody = [this](const Entity e, bool isDynamicBody) {
                if (!world.IsAlive(e) || !world.IsActiveInHierarchy(e)) {
                    return;
                }

                BodyCache body{};
                body.entity = e;
                body.packed = ECS::EntityUtils::Pack(e);
                body.isDynamic = isDynamicBody;
                body.local = world.TryGet<Components::LocalTransform>(e);
                body.rb = world.TryGet<Components::Rigidbody2D>(e);
                body.vel = world.TryGet<Components::LinearVelocity2D>(e);
                body.angVel = world.TryGet<Components::AngularVelocity2D>(e);
                body.circle = world.TryGet<Components::CircleCollider2D>(e);
                body.box = world.TryGet<Components::BoxCollider2D>(e);
                if (!body.local || (!body.circle && !body.box)) {
                    return;
                }

                const auto* layer = world.TryGet<Components::Layer>(e);
                if (!layer) {
                    if (!loggedMissingLayer) {
                        loggedMissingLayer = true;
                        LOG_WARNING("PhysicsSystem: Skipping collision checks (missing Layer component on one or more entities).");
                    }
                    return;
                }

                body.layerId = layer->Id;
                body.layerMask = layerManager.GetLayerMask(body.layerId);
                if (!layerManager.Get(body.layerId).physicsEnabled) {
                    return;
                }

                body.hasPhysicsPair = (body.rb != nullptr && body.vel != nullptr);
                if (const auto* m = world.TryGet<Components::PhysicsMaterial2D>(e)) {
                    body.material = *m;
                }
                body.isTrigger = (body.circle && (body.circle->Flags & 0x1u)) || (body.box && (body.box->Flags & 0x1u));
                if (!RefreshBodyWorldShape(body)) {
                    return;
                }

                const size_t index = bodies.size();
                bodies.push_back(body);
                bodyByPacked[body.packed] = index;
                if (body.isDynamic) {
                    dynamicPacked.insert(body.packed);
                }
            };

            for (const Entity e : dynamicEntities) {
                addBody(e, true);
            }
            for (const Entity e : staticEntities) {
                addBody(e, false);
            }
        }

        void BuildBroadphasePairs() {
            broadphasePairs.clear();
            broadphasePairs.reserve(dynamicEntities.size() * 4);

            SpatialPartitioning grid;
            for (const BodyCache& body : bodies) {
                if (!body.hasWorldShape) {
                    continue;
                }
                if (body.circle) {
                    grid.Insert(body.entity, Vector3D(body.worldCircle.Center.X, body.worldCircle.Center.Y, 0.0f), body.worldCircle.Radius);
                }
                else {
                    grid.InsertBox(body.entity, Vector3D(body.worldAabb.Center.X, body.worldAabb.Center.Y, 0.0f), body.worldAabb.HalfExtents);
                }
            }

            std::unordered_set<PackedEntityPair, PackedEntityPairHash> seen;
            seen.reserve(dynamicEntities.size() * 4);

            for (const auto& cell : grid.Grid()) {
                const auto& entities = cell.second;
                for (size_t i = 0; i + 1 < entities.size(); ++i) {
                    for (size_t j = i + 1; j < entities.size(); ++j) {
                        const PackedEntityId packedA = ECS::EntityUtils::Pack(entities[i]);
                        const PackedEntityId packedB = ECS::EntityUtils::Pack(entities[j]);
                        if (!dynamicPacked.contains(packedA) && !dynamicPacked.contains(packedB)) {
                            continue;
                        }
                        const PackedEntityPair pairKey = MakeCollisionPair(packedA, packedB);
                        if (seen.insert(pairKey).second) {
                            broadphasePairs.push_back(pairKey);
                        }
                    }
                }
            }

            std::sort(broadphasePairs.begin(), broadphasePairs.end(), [](const PackedEntityPair& lhs, const PackedEntityPair& rhs) {
                if (lhs.A != rhs.A) {
                    return lhs.A < rhs.A;
                }
                return lhs.B < rhs.B;
            });
        }

        void SolvePairs(int solverIterations) {
            for (int it = 0; it < solverIterations; ++it) {
                int resolvedCount = 0;

                for (BodyCache& body : bodies) {
                    RefreshBodyWorldShape(body);
                }

                for (const PackedEntityPair& pairId : broadphasePairs) {
                    const auto itA = bodyByPacked.find(pairId.A);
                    const auto itB = bodyByPacked.find(pairId.B);
                    if (itA == bodyByPacked.end() || itB == bodyByPacked.end()) {
                        continue;
                    }

                    BodyCache& a = bodies[itA->second];
                    BodyCache& b = bodies[itB->second];
                    if (!a.hasWorldShape || !b.hasWorldShape) {
                        continue;
                    }
                    if (!world.IsAlive(a.entity) || !world.IsAlive(b.entity)) {
                        continue;
                    }
                    if (!Engine::CanCollide(a.layerMask, a.layerId, b.layerMask, b.layerId)) {
                        continue;
                    }

                    Engine::Collision::ContactManifold manifold;
                    if (!BuildContactManifold(a, b, manifold)) {
                        continue;
                    }

                    if (a.isTrigger || b.isTrigger) {
                        if (a.isTrigger) {
                            currentTriggerOverlaps.insert(MakeTriggerPair(a.packed, b.packed));
                        }
                        if (b.isTrigger) {
                            currentTriggerOverlaps.insert(MakeTriggerPair(b.packed, a.packed));
                        }
                        continue;
                    }

                    if (!a.hasPhysicsPair && !b.hasPhysicsPair) {
                        if (!loggedMissingPhysicsPair) {
                            loggedMissingPhysicsPair = true;
                            LOG_WARNING("PhysicsSystem: Skipping collision resolution (no Rigidbody2D+LinearVelocity2D pair found).");
                        }
                        continue;
                    }

                    const bool firstSeenThisFrame = frameCollisions.insert(pairId).second;
                    const bool isNewCollision = !previousCollisions.contains(pairId);
                    if (firstSeenThisFrame && isNewCollision) {
                        eventDispatcher.FireCollisionEvent(
                            a.packed,
                            b.packed,
                            Vector3D(manifold.points[0].X, manifold.points[0].Y, 0.0f),
                            Vector3D(manifold.normal.X, manifold.normal.Y, 0.0f),
                            Vector3D(0.0f, 0.0f, 0.0f),
                            0.0f);
                        ++collisionEnterCount;
                    }

                    Components::Rigidbody2D rbA{};
                    Components::Rigidbody2D rbB{};
                    Components::LinearVelocity2D vA{};
                    Components::LinearVelocity2D vB{};
                    if (a.rb) rbA = *a.rb;
                    if (b.rb) rbB = *b.rb;
                    if (a.vel) vA = *a.vel;
                    if (b.vel) vB = *b.vel;

                    const Components::PhysicsMaterial2D material = CombineMaterials(a.material, b.material);
                    Engine::Physics::ResolveCollisionManifold(rbA, rbB, vA, vB, *a.local, *b.local, manifold, material);
                    if (a.vel) *a.vel = vA;
                    if (b.vel) *b.vel = vB;
                    ++resolvedCount;
                }

                if (resolvedCount == 0) {
                    break;
                }
            }
        }

        void ResolveTilemapCollisions() {
            if (system.m_runtimeTileMaps.empty()) {
                return;
            }

            constexpr float kTileCoordEpsilon = 1e-4f;
            const Components::PhysicsMaterial2D tileMaterial{ 0.2f, 0.5f, 0.5f };

            for (BodyCache& body : bodies) {
                if (!body.isDynamic || body.isTrigger || !body.hasPhysicsPair || !body.rb || !body.vel || body.rb->Mass <= 0.0f) {
                    continue;
                }
                if (!RefreshBodyWorldShape(body)) {
                    continue;
                }

                for (const auto& entryPair : system.m_runtimeTileMaps) {
                    const PhysicsSystem::RuntimeTileMapEntry& entry = entryPair.second;
                    if (!entry.Enabled || !entry.Map || entry.Map->LayerCount() == 0) {
                        continue;
                    }
                    if (!layerManager.Get(entry.LayerId).physicsEnabled) {
                        continue;
                    }

                    const uint32_t tileLayerMask = layerManager.GetLayerMask(entry.LayerId);
                    if (!Engine::CanCollide(body.layerMask, body.layerId, tileLayerMask, entry.LayerId)) {
                        continue;
                    }

                    const float tileSize = entry.Map->TileSize();
                    if (tileSize <= 0.0f) {
                        continue;
                    }

                    const float minX = body.worldAabb.Center.X - body.worldAabb.HalfExtents.X;
                    const float maxX = body.worldAabb.Center.X + body.worldAabb.HalfExtents.X;
                    const float minY = body.worldAabb.Center.Y - body.worldAabb.HalfExtents.Y;
                    const float maxY = body.worldAabb.Center.Y + body.worldAabb.HalfExtents.Y;

                    int32_t tileMinX = entry.Map->WorldToTileSigned(minX - entry.Origin.X);
                    int32_t tileMinY = entry.Map->WorldToTileSigned(minY - entry.Origin.Y);
                    int32_t tileMaxX = entry.Map->WorldToTileSigned(maxX - entry.Origin.X - kTileCoordEpsilon);
                    int32_t tileMaxY = entry.Map->WorldToTileSigned(maxY - entry.Origin.Y - kTileCoordEpsilon);

                    if (tileMaxX < tileMinX) std::swap(tileMaxX, tileMinX);
                    if (tileMaxY < tileMinY) std::swap(tileMaxY, tileMinY);

                    const float tileHalf = tileSize * 0.5f;
                    const float subHalf = tileSize * 0.25f;
                    const Vector2D subHalfExtents(subHalf, subHalf);
                    const Components::PhysicsMaterial2D material = CombineMaterials(body.material, tileMaterial);

                    auto resolveTileCell = [this, &body, material](const Vector2D& cellCenter, const Vector2D& halfExtents) {
                        Engine::WorldOBB tileObb;
                        tileObb.Center = cellCenter;
                        tileObb.HalfExtents = halfExtents;
                        tileObb.Rotation = 0.0f;
                        tileObb.AxisX = Vector2D(1.0f, 0.0f);
                        tileObb.AxisY = Vector2D(0.0f, 1.0f);

                        Engine::Collision::ContactManifold manifold;
                        if (body.circle) {
                            Vector2D n{};
                            float depth = 0.0f;
                            Vector2D contact{};
                            if (!TestCircleBox(body.worldCircle, tileObb, n, depth, contact)) {
                                return;
                            }
                            manifold.normal = n;
                            manifold.penetration = depth;
                            manifold.points[0] = contact;
                            manifold.pointCount = 1;
                        }
                        else {
                            manifold = TestBoxBox(body.worldObb, tileObb);
                            if (manifold.pointCount <= 0) {
                                return;
                            }
                        }

                        Components::Rigidbody2D staticRb{};
                        staticRb.Mass = 0.0f;
                        Components::LinearVelocity2D staticVel{};
                        Components::LocalTransform staticXf{};
                        staticXf.Position = { cellCenter.X, cellCenter.Y, 0.0f };
                        staticXf.Scale = { 1.0f, 1.0f, 1.0f };
                        staticXf.Rotation = Quaternion::Identity();

                        Components::Rigidbody2D dynamicRb = *body.rb;
                        Components::LinearVelocity2D dynamicVel = *body.vel;
                        Engine::Physics::ResolveCollisionManifold(dynamicRb, staticRb, dynamicVel, staticVel, *body.local, staticXf, manifold, material);
                        *body.vel = dynamicVel;
                        RefreshBodyWorldShape(body);
                    };

                    for (int32_t ty = tileMinY; ty <= tileMaxY; ++ty) {
                        for (int32_t tx = tileMinX; tx <= tileMaxX; ++tx) {
                            if (entry.Map->GetTileSigned(0, tx, ty) == EMPTY_TILE) {
                                continue;
                            }

                            const uint8_t mask = static_cast<uint8_t>(entry.Map->GetCollisionMaskSigned(tx, ty) & 0x0F);
                            if (mask == 0) {
                                continue;
                            }

                            const float tileWorldX = entry.Origin.X + entry.Map->TileToWorldSigned(tx);
                            const float tileWorldY = entry.Origin.Y + entry.Map->TileToWorldSigned(ty);

                            if (mask == 0x0F) {
                                resolveTileCell(Vector2D(tileWorldX + tileHalf, tileWorldY + tileHalf), Vector2D(tileHalf, tileHalf));
                                continue;
                            }

                            if (mask == (kCollisionMaskBottomLeft | kCollisionMaskBottomRight)) {
                                resolveTileCell(Vector2D(tileWorldX + tileHalf, tileWorldY + subHalf), Vector2D(tileHalf, subHalf));
                                continue;
                            }
                            if (mask == (kCollisionMaskTopLeft | kCollisionMaskTopRight)) {
                                resolveTileCell(Vector2D(tileWorldX + tileHalf, tileWorldY + tileSize - subHalf), Vector2D(tileHalf, subHalf));
                                continue;
                            }
                            if (mask == (kCollisionMaskBottomLeft | kCollisionMaskTopLeft)) {
                                resolveTileCell(Vector2D(tileWorldX + subHalf, tileWorldY + tileHalf), Vector2D(subHalf, tileHalf));
                                continue;
                            }
                            if (mask == (kCollisionMaskBottomRight | kCollisionMaskTopRight)) {
                                resolveTileCell(Vector2D(tileWorldX + tileSize - subHalf, tileWorldY + tileHalf), Vector2D(subHalf, tileHalf));
                                continue;
                            }

                            if (mask & kCollisionMaskBottomLeft) {
                                resolveTileCell(Vector2D(tileWorldX + subHalf, tileWorldY + subHalf), subHalfExtents);
                            }
                            if (mask & kCollisionMaskBottomRight) {
                                resolveTileCell(Vector2D(tileWorldX + tileSize - subHalf, tileWorldY + subHalf), subHalfExtents);
                            }
                            if (mask & kCollisionMaskTopLeft) {
                                resolveTileCell(Vector2D(tileWorldX + subHalf, tileWorldY + tileSize - subHalf), subHalfExtents);
                            }
                            if (mask & kCollisionMaskTopRight) {
                                resolveTileCell(Vector2D(tileWorldX + tileSize - subHalf, tileWorldY + tileSize - subHalf), subHalfExtents);
                            }
                        }
                    }
                }
            }
        }

        void FinalizeEvents() {
            for (const PackedEntityPair& pairId : previousCollisions) {
                if (frameCollisions.contains(pairId)) {
                    continue;
                }

                const Entity entityA = ECS::EntityUtils::Unpack(pairId.A);
                const Entity entityB = ECS::EntityUtils::Unpack(pairId.B);
                if (!world.IsAlive(entityA) || !world.IsAlive(entityB)) {
                    continue;
                }

                eventDispatcher.FireCollisionExitEvent(
                    ECS::EntityUtils::Pack(entityA),
                    ECS::EntityUtils::Pack(entityB),
                    Vector3D(0.0f, 0.0f, 0.0f));
                ++collisionExitCount;
            }

            previousCollisions = std::move(frameCollisions);

            for (const PackedEntityPair& pairId : currentTriggerOverlaps) {
                const bool wasOverlapping = previousTriggerOverlaps.contains(pairId);
                const Entity triggerEntity = ECS::EntityUtils::Unpack(pairId.A);
                const Entity otherEntity = ECS::EntityUtils::Unpack(pairId.B);
                if (!world.IsAlive(triggerEntity) || !world.IsAlive(otherEntity)) {
                    continue;
                }

                if (wasOverlapping) {
                    eventDispatcher.FireTriggerStayEvent(ECS::EntityUtils::Pack(triggerEntity), ECS::EntityUtils::Pack(otherEntity));
                    ++triggerStayCount;
                }
                else {
                    eventDispatcher.FireTriggerEnterEvent(ECS::EntityUtils::Pack(triggerEntity), ECS::EntityUtils::Pack(otherEntity));
                    ++triggerEnterCount;
                }
            }

            for (const PackedEntityPair& pairId : previousTriggerOverlaps) {
                if (currentTriggerOverlaps.contains(pairId)) {
                    continue;
                }
                const Entity triggerEntity = ECS::EntityUtils::Unpack(pairId.A);
                const Entity otherEntity = ECS::EntityUtils::Unpack(pairId.B);
                if (!world.IsAlive(triggerEntity) || !world.IsAlive(otherEntity)) {
                    continue;
                }
                eventDispatcher.FireTriggerExitEvent(ECS::EntityUtils::Pack(triggerEntity), ECS::EntityUtils::Pack(otherEntity));
                ++triggerExitCount;
            }

            previousTriggerOverlaps = std::move(currentTriggerOverlaps);
            (void)collisionEnterCount;
            (void)collisionExitCount;
            (void)triggerEnterCount;
            (void)triggerExitCount;
            (void)triggerStayCount;
        }

        PhysicsSystem& system;
        World& world;
        Scenes::LayerManager& layerManager;
        float dt = 0.0f;
        ECS::Events::EventDispatcher eventDispatcher;

        std::vector<Entity> dynamicEntities;
        std::vector<Entity> staticEntities;
        std::vector<BodyCache> bodies;
        std::unordered_map<PackedEntityId, size_t> bodyByPacked;
        std::unordered_set<PackedEntityId> dynamicPacked;
        std::vector<PackedEntityPair> broadphasePairs;

        std::unordered_set<PackedEntityPair, PackedEntityPairHash> frameCollisions;
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> currentTriggerOverlaps;
        static std::unordered_set<PackedEntityPair, PackedEntityPairHash> previousCollisions;
        static std::unordered_set<PackedEntityPair, PackedEntityPairHash> previousTriggerOverlaps;

        bool loggedMissingLayer = false;
        bool loggedMissingPhysicsPair = false;
        int collisionEnterCount = 0;
        int collisionExitCount = 0;
        int triggerEnterCount = 0;
        int triggerExitCount = 0;
        int triggerStayCount = 0;
    };

    std::unordered_set<PackedEntityPair, PackedEntityPairHash> PhysicsPipelineRunner::previousCollisions;
    std::unordered_set<PackedEntityPair, PackedEntityPairHash> PhysicsPipelineRunner::previousTriggerOverlaps;

    /**
     * @brief Execute one full frame of the internal staged physics pipeline.
     */
    void RunPhysicsPipeline(PhysicsSystem& system, World& world, Scenes::LayerManager& layerManager, float dt) {
        PhysicsPipelineRunner pipeline(system, world, layerManager, dt);
        pipeline.Run();
    }
}  // namespace ECS
