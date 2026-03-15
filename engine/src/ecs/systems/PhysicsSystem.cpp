/* Start Header *****************************************************************/
/*!
\file   PhysicsSystem.cpp
\author Dalton Koh Shi Hao (80%)
        Muhammad Nur Fadzly Bin Zulkifli (10%)
        Foo Rui Qin (10%)
\par    d.koh@digipen.edu
        muhammadnurfadzly.b@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026

\brief
Broad/narrow-phase utilities and per-frame 2D physics update loop.

\details
This translation unit implements the main 2D physics system for the ECS.
Responsibilities include:
- Spatial hashing grid (broad phase) to prune collision checks
- Shape tests (circle-circle, box-box, circle-box) composing narrow phase
- Time integration for dynamic bodies (linear + angular)
- Optional world-boundary constraint application
- Iterative position correction and velocity resolution using Physics helpers

The implementation favors clarity and robustness with early-outs and explicit
checks. It relies on plain ECS components and engine physics helpers for
reusable math and manifold building.

\sources
https://saeed1262.github.io/blog/2025/spatial-hashing-collision/
break down of implementing of spatial hashing method
linking it to broadphase collisions checking
finishing off with quick speed narrow phase checking
overall optimise collision checks to a low amount
making this a systematic autonomous approach to collision
systems

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include "ecs/systems/PhysicsSystem.h"
#include "services/TimeSystem.h"
#include "physics/Collision.h"
#include "physics/Physics.h"
#include "physics/LayerMask.h"
#include "scene/LayerManager.h"
#include "ecs/Components.h"
#include "core/World/TileMap.hpp"
#include "core/World/TileTypes.hpp"
#include "core/ProjectPaths.h"
#include "ecs/StringTable.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <filesystem>
#include "helpers/MathUtils.h"
#include "helpers/EntityUtils.h"
#include "helpers/TransformUtils.h"
#include <iostream>
#include "core/Logger.h"
#include "audio/FmodAudioDevice.h"
#include "ecs/events/EventComponents.h"
#include "ecs/events/EventDispatcher.h"
#include "math/Vector3D.h"

extern Audio::FmodAudioDevice* gAudioDevice;

#ifndef PHYSICS_AUDIO_DEVICE
#define PHYSICS_AUDIO_DEVICE (gAudioDevice)
#endif

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

    // Resolve a project-relative path to an absolute path for loading assets
    std::string ResolveProjectPathForLoad(const std::string& path) {

        // If path is empty or project paths not initialized, return as-is (likely to fail later)
        if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
            return path;
        }

        // If already absolute, return as-is
        std::filesystem::path fsPath(path);
        if (fsPath.is_absolute()) {
            return path;
        }

        // Otherwise, resolve relative to project root
        std::filesystem::path absolute = Engine::ProjectPaths::ToAbsolutePath(path);
        return absolute.lexically_normal().string();
    }

    // Helper to get the effective transform for runtime tilemaps
    static void GetTileMapTransform(ECS::World& world, const ECS::Entity entity, const ECS::Components::LocalTransform& lt, 
        Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) 
    {

        // If the entity has a WorldTransform, decompose it to get the effective position/rotation/scale
        if (world.Has<ECS::Components::WorldTransform>(entity)) {
            const auto& wt = world.Get<ECS::Components::WorldTransform>(entity);
            TransformUtils::DecomposeTRS(wt.Matrix, outPosition, outRotation, outScale);
        }

        // Otherwise use LocalTransform directly when world transform is unavailable
        else {
            outPosition = lt.Position;
            outRotation = lt.Rotation;
            outScale = lt.Scale;
        }
    }
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

    while (!parent.IsNull()) {
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

    // Declares scheduler metadata including component access requirements and run policy
    SystemMetadata PhysicsSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Physics");

        // Read accesses
        builder.ReadComponent<Components::LocalTransform>();
        builder.ReadComponent<Components::CircleCollider2D>();
        builder.ReadComponent<Components::BoxCollider2D>();
        builder.ReadComponent<Components::Rigidbody2D>();
        builder.ReadComponent<Components::Active>();
        builder.ReadComponent<Components::Parent>();

        // Write accesses
        builder.WriteComponent<Components::LocalTransform>();
        builder.WriteComponent<Components::Rigidbody2D>();

        // Execution parameters
        builder.SetExecutionOrder(0);
        builder.SetGroup(SystemGroup::Physics);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    // Clears cached collision and tilemap runtime state when system is destroyed
    void PhysicsSystem::OnDestroy(World& /*world*/) {
        m_previousCollisions.clear();
        m_previousTriggerOverlaps.clear();
        m_runtimeTileMaps.clear();
    }

    // Refreshes runtime tilemap cache so physics uses current tilemap components and paths
    void PhysicsSystem::RefreshRuntimeTileMaps(World& world) {

        // Track entities observed this frame so stale cache entries can be removed later
        std::unordered_set<EntityId> seen;

        // Iterate tilemap components and update or rebuild cache entries as needed
        world.Each<ECS::Components::TileMapComponent>([this, &seen, &world](const ECS::Entity entity, ECS::Components::TileMapComponent& comp) {

            // Mark entity as seen so we can keep only active tilemap entries
            seen.insert(entity.Index);

            // Lookup or create cache entry keyed by entity id
            RuntimeTileMapEntry& entry = m_runtimeTileMaps[entity.Index];

            // Generation mismatch means the component was replaced or structurally updated
            const bool generationChanged = (entry.Generation != entity.Generation);

            // Reset entry on generation change so all derived fields rebuild from fresh component data
            if (generationChanged) {
                entry = RuntimeTileMapEntry{};
            }

            // Cache current generation value for change detection in subsequent frames
            entry.Generation = entity.Generation;

            // Resolve path token into loadable project absolute path
            std::string mapPath = ECS::StringTable::Resolve(comp.TileMapPath);
            mapPath = ResolveProjectPathForLoad(mapPath);

            // Reload map when path dimensions tile size or generation diverge from cache
            const bool mapNeedsReload = generationChanged || entry.MapPath != mapPath || entry.TileWorldSize != comp.TileWorldSize ||
                entry.DefaultWidth != comp.DefaultWidth || entry.DefaultHeight != comp.DefaultHeight;

            // Rebuild map handle and metadata when reload conditions are met
            if (mapNeedsReload) {
                entry.Map.reset();
                entry.MapPath = mapPath;
                entry.TileWorldSize = comp.TileWorldSize;
                entry.DefaultWidth = comp.DefaultWidth;
                entry.DefaultHeight = comp.DefaultHeight;

                // Try loading from disk when path exists
                if (!mapPath.empty() && std::filesystem::exists(mapPath)) {
                    entry.Map = std::make_shared<TileMap>(comp.TileWorldSize);

                    // Reset on load failure so fallback initialization path can create a valid map
                    if (!entry.Map->LoadMap(mapPath)) {
                        entry.Map.reset();
                    }
                }

                // Fallback map prevents null dereference in physics loops when asset load fails
                if (!entry.Map) {
                    entry.Map = std::make_shared<TileMap>(comp.TileWorldSize);
                    entry.Map->AddLayer(comp.DefaultWidth, comp.DefaultHeight);
                }
            }

            // Compute tilemap origin in world space from effective transform
            Vector2D origin{ 0.0f, 0.0f };

            // Use effective transform path so parented tilemaps collide at correct world coordinates
            if (world.Has<ECS::Components::LocalTransform>(entity)) {
                const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
                Vector3D position, scale;
                Quaternion rotation;
                GetTileMapTransform(world, entity, lt, position, rotation, scale);
                origin = Vector2D(position.X, position.Y);
            }

            // Cache origin plus enabled state and layer id for tile collision checks
            entry.Origin = origin;
            entry.Enabled = comp.Visible && world.IsActiveInHierarchy(entity);
            entry.LayerId = world.Has<ECS::Components::Layer>(entity) ? world.Get<ECS::Components::Layer>(entity).Id : 0;
        });

        // Remove cache entries for entities that no longer expose TileMapComponent
        for (auto it = m_runtimeTileMaps.begin(); it != m_runtimeTileMaps.end(); ) {
            if (!seen.contains(it->first)) {
                it = m_runtimeTileMaps.erase(it);
            }
            else {
                ++it;
            }
        }
    }

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

    // =====================================================================
    // PhysicsSystem::Update (main per-frame step)
    // =====================================================================

    // Runs full physics frame integration broad-phase narrow-phase resolution and event dispatch
    void PhysicsSystem::OnUpdate(World& world) {
        if (!Engine::Physics::IsEnabled()) return;

        const float frameDt = static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());
        const float fixedDt = static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
        if (frameDt <= 0.0f || fixedDt <= 0.0f) return;

        constexpr int kMaxFixedStepsPerFrame = 5;
        static float s_fixedAccumulator = 0.0f;
        s_fixedAccumulator = std::min(s_fixedAccumulator + frameDt, fixedDt * static_cast<float>(kMaxFixedStepsPerFrame));
        if (s_fixedAccumulator < fixedDt) return;

        // Sync runtime tilemap cache once per frame before fixed simulation steps.
        RefreshRuntimeTileMaps(world);

        static bool s_collisionCueLoaded = false;
        if (!s_collisionCueLoaded) {
            if (auto* audio = PHYSICS_AUDIO_DEVICE) {
                const std::string cue = "sfx_collide";
                const std::string path = std::filesystem::absolute("assets/Audio/SFX/Squishy-Splatter_1.wav").string();
                Audio::SoundParams sp;
                sp.Stream = false;
                sp.Is3D = false;
                audio->LoadCue(cue, path, sp);
                s_collisionCueLoaded = true;
            }
        }

        int fixedStepsExecuted = 0;
        while (s_fixedAccumulator >= fixedDt && fixedStepsExecuted < kMaxFixedStepsPerFrame) {
            s_fixedAccumulator -= fixedDt;
            ++fixedStepsExecuted;

            const float dt = fixedDt;

            int collisionEnterCount = 0;
            int collisionExitCount = 0;
            int triggerEnterCount = 0;
            int triggerExitCount = 0;
            int triggerStayCount = 0;
            bool loggedMissingLayer = false;
            bool loggedMissingPhysicsPair = false;

            // =====================
            // Simulation Settings
            // =====================
            constexpr int kSubsteps = 4;
            const int substeps = kSubsteps;
            const float subDt = dt / static_cast<float>(substeps);

            // Create event dispatcher for firing collision events
            ECS::Events::EventDispatcher eventDispatcher(&world);

            // Track collisions and trigger overlaps for the whole frame (across substeps)
            std::unordered_set<PackedEntityPair, PackedEntityPairHash> frameCollisions;
            std::unordered_set<PackedEntityPair, PackedEntityPairHash> currentTriggerOverlaps;

            // Running frame counter to reset per-frame SFX dedupe
            static uint64_t s_frameCounter = 0;
            ++s_frameCounter;

            // Get LayerManager for layer-wide physics gating
            auto* layerManager = world.GetLayerManager();

        // =====================
        // Entity Collection
        // =====================

        // Collect entity sets once per frame (usually fine)
        // AngularVelocity2D, Layer, and Active are now optional
        std::vector<Entity> dynamicEntities;
        dynamicEntities.reserve(512);

        // Iterates all entities that have rigidbody, linear velocity, and transform
        // Optional: AngularVelocity2D (for rotation), Layer (for collision filtering), Active (for enable/disable)
        world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::LocalTransform>(
            [&](const Entity e,
                const Components::Rigidbody2D& rb,
                Components::LinearVelocity2D&,
                Components::LocalTransform&) {
                    if (!world.IsActiveInHierarchy(e)) {
                        return;
                    }
                    
                    if (rb.Mass <= 0.0f) return; // only dynamics here

                    // === Layer-wide physics gating (optional) ===


                    if (layerManager) {
                        const auto* layer = world.TryGet<Components::Layer>(e);
                        const uint16_t layerId = layer ? layer->Id : 0;
                        const auto& layerData = layerManager->Get(layerId);
                        if (!layerData.physicsEnabled)
                            return;  // Skip physics simulation for this layer
                    }

                    // Must have some collider to participate in broad-phase
                    if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) return;
                    dynamicEntities.push_back(e);
            });

        // Find and stores static entities
        std::vector<Entity> staticEntities;
        staticEntities.reserve(128);
        world.Each<Components::Rigidbody2D, Components::LocalTransform>(
            [&](const Entity e, const Components::Rigidbody2D& rb, const Components::LocalTransform&) {
                if (rb.Mass > 0.0f) return; // only statics here
                if (!world.IsActiveInHierarchy(e)) {
                    return;
                }

                // Layer-wide physics gating for static entities
                if (layerManager) {
                    const auto* layer = world.TryGet<Components::Layer>(e);
                    const uint16_t layerId = layer ? layer->Id : 0;
                    const auto& layerData = layerManager->Get(layerId);
                    if (!layerData.physicsEnabled)
                        return;  // Skip physics simulation for this layer
                }
                
                if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e)) return;
                staticEntities.push_back(e); // Push to store and use static entities for checks later
            });

        std::unordered_set<EntityId> broadphaseIds;
        broadphaseIds.reserve(dynamicEntities.size() + staticEntities.size());
        for (const auto& e : dynamicEntities) broadphaseIds.insert(e.Index);
        for (const auto& e : staticEntities) broadphaseIds.insert(e.Index);

        // Include non-rigidbody colliders (triggers or static colliders without a Rigidbody2D)
        world.Each<Components::LocalTransform>([&](const Entity e, const Components::LocalTransform&) {
            if (broadphaseIds.find(e.Index) != broadphaseIds.end())
                return;

            if (!world.IsActiveInHierarchy(e)) {
                return;
            }

            if (layerManager) {
                const auto* layer = world.TryGet<Components::Layer>(e);
                const uint16_t layerId = layer ? layer->Id : 0;
                const auto& layerData = layerManager->Get(layerId);
                if (!layerData.physicsEnabled)
                    return;
            }

            if (!world.Has<Components::CircleCollider2D>(e) && !world.Has<Components::BoxCollider2D>(e))
                return;

            if (world.Has<Components::Rigidbody2D>(e))
                return;

            staticEntities.push_back(e);
            broadphaseIds.insert(e.Index);
        });

        // Substep loop: integrate and resolve in smaller time slices
        for (int step = 0; step < substeps; ++step) {

            // Integrate dynamics with subDt and apply optional world bounds
            // AngularVelocity2D is optional
            world.Each<Components::Rigidbody2D, Components::LinearVelocity2D, Components::LocalTransform>(
                    [&](const Entity e,
                        const Components::Rigidbody2D& rb,
                        Components::LinearVelocity2D& linVel,
                        Components::LocalTransform& xf)
                    {
                        if (!world.IsActiveInHierarchy(e)) {
                            return;
                        }
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

                        // Angular integration (optional AngularVelocity2D)
                        if (!(rb.Flags & (1 << 2))) {  // if not fixed rotation
                            if (auto* angVel = world.TryGet<Components::AngularVelocity2D>(e)) {
                                const float angAcc = Engine::Physics::CalculateAngularAcceleration(rb, *angVel);
                                if (std::abs(angAcc * subDt) > std::abs(angVel->Value)) angVel->Value = 0.0f;
                                else angVel->Value += angAcc * subDt;
                                xf.Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, angVel->Value * subDt) * xf.Rotation;
                            }
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

            // =====================
            // Broad Phase (Uniform Grid)
            // =====================

            // Rebuild grid each substep because poses changed
            std::unordered_map<EntityId, Components::LocalTransform> worldTransformCache;
            worldTransformCache.reserve(dynamicEntities.size() + staticEntities.size());

            auto getWorldTransformCached = [&](const Entity e, Components::LocalTransform& outTransform) -> bool {
                auto it = worldTransformCache.find(e.Index);
                if (it != worldTransformCache.end()) {
                    outTransform = it->second;
                    return true;
                }

                Components::LocalTransform computed{};
                if (!GetPhysicsWorldTransform(world, e, computed)) {
                    return false;
                }

                worldTransformCache.emplace(e.Index, computed);
                outTransform = computed;
                return true;
            };

            auto refreshWorldTransformCache = [&](const Entity e) {
                Components::LocalTransform updated{};
                if (GetPhysicsWorldTransform(world, e, updated)) {
                    worldTransformCache[e.Index] = updated;
                }
                else {
                    worldTransformCache.erase(e.Index);
                }
            };

            SpatialPartitioning grid;
            auto insertEntity = [&](Entity e) {
                Components::LocalTransform worldTransform{};
                if (!getWorldTransformCached(e, worldTransform)) return;
                if (const auto* c = world.TryGet<Components::CircleCollider2D>(e)) {

                    // Step 2: Use world-space circle (includes scale and offset)
                    Engine::WorldCircle wc = Engine::Physics::GetWorldCircle(*c, worldTransform);
                    grid.Insert(e, Vector3D(wc.Center.X, wc.Center.Y, 0.0f), wc.Radius);
                }
                else if (const auto* b = world.TryGet<Components::BoxCollider2D>(e)) {

                    // Step 2: Use world-space AABB (includes scale and offset)
                    Engine::WorldAABB wa = Engine::Physics::GetWorldAABB(*b, worldTransform);
                    grid.InsertBox(e, Vector3D(wa.Center.X, wa.Center.Y, 0.0f), wa.HalfExtents);
                }
            };
            for (Entity e : dynamicEntities) insertEntity(e);
            for (Entity e : staticEntities)  insertEntity(e);

            // =====================
            // Pair Generation
            // =====================

            // Candidate pairs deduped per substep
            std::vector<std::pair<Entity, Entity>> pairs;
            std::unordered_set<PackedEntityPair, PackedEntityPairHash> seen;
            pairs.reserve(dynamicEntities.size() * 4);
            seen.reserve(dynamicEntities.size() * 4);

            // Builds a list of unique candidate collision pairs from each spatial - grid cell, deduplicating pairs that appear in multiple cells
            // Iterates all occupied cells in the spatial hash/grid. Each cell has a small list of entities that overlap that cell
            for (const auto& cell : grid.Grid()) {
                const auto& ents = cell.second;

                // Enumerate all unordered pairs within the cell by running i from 0..n-2 and j from i+1..n-1
                for (size_t i = 0; i + 1 < ents.size(); ++i) {
                    for (size_t j = i + 1; j < ents.size(); ++j) {

                        //Packs the pair (ents[i], ents[j]) into the 64-bit canonical key using pairKey
                        const PackedEntityPair key = MakeCollisionPair(
                            ECS::EntityUtils::Pack(ents[i]),
                            ECS::EntityUtils::Pack(ents[j]));
                        if (seen.insert(key).second) pairs.emplace_back(ents[i], ents[j]);
                    }
                }
            }

            // =====================
            // Narrow Phase + Resolution
            // =====================

            // You can reduce the inner iterative solver because substeps already help stability
            constexpr int kSolverIters = 3;
            const int solverIters = kSolverIters;

            // run several small correction passes to improve stability
            for (int it = 0; it < solverIters; ++it) {
                int resolved = 0;

                // Iterate all broad-phase candidate pairs (A,B)
                for (auto [A, B] : pairs) {

                    // Skip if either entity got destroyed during earlier steps
                    if (!world.IsAlive(A) || !world.IsAlive(B)) continue;

                    // Fetch transforms; narrow phase needs world-space poses
                    auto* tA = world.TryGet<Components::LocalTransform>(A);
                    auto* tB = world.TryGet<Components::LocalTransform>(B);

                    // cannot resolve without positions
                    if (!tA || !tB) continue;
                    Components::LocalTransform tAWorld{};
                    Components::LocalTransform tBWorld{};
                    if (!getWorldTransformCached(A, tAWorld)) continue;
                    if (!getWorldTransformCached(B, tBWorld)) continue;

                    // Honor Active flags (including parents): if disabled, skip
                    if (!world.IsActiveInHierarchy(A)) continue;
                    if (!world.IsActiveInHierarchy(B)) continue;

                    // Query collider shapes present on each entity
                    const auto* circA = world.TryGet<Components::CircleCollider2D>(A);
                    const auto* boxA = world.TryGet<Components::BoxCollider2D>(A);
                    const auto* circB = world.TryGet<Components::CircleCollider2D>(B);
                    const auto* boxB = world.TryGet<Components::BoxCollider2D>(B);

                    // If either side has no collider, this pair cannot collide
                    if ((!circA && !boxA) || (!circB && !boxB)) continue;

                    // =====================
                    // Layer Mask
                    // =====================

                    // Check if both entities have Layer components
                    const auto* la = world.TryGet<Components::Layer>(A);
                    const auto* lb = world.TryGet<Components::Layer>(B);

                    // If either entity is missing a Layer component, skip collision
                    // (Entities must be explicitly assigned to a layer to participate in layer-based collision)
                    if (!la || !lb)
                    {
                        if (!loggedMissingLayer)
                        {
                            loggedMissingLayer = true;
                            LOG_WARNING("PhysicsSystem: Skipping collision event (missing Layer component on one or both entities).");
                        }
                        continue;
                    }
                    
                    uint16_t layerAId = la->Id;
                    uint16_t layerBId = lb->Id;

                    // Read collision masks directly from LayerManager (not from collider components)
                    // This ensures we always use current, authoritative layer collision settings
                    // regardless of whether collider masks have been synced yet
                    uint32_t maskA = layerManager->GetLayerMask(layerAId);
                    uint32_t maskB = layerManager->GetLayerMask(layerBId);

                    // If masks/layers indicate no collision, skip early
                    if (!Engine::CanCollide(maskA, layerAId, maskB, layerBId))
                        continue;

                    // Narrow phase: run the appropriate shape test to get contact normal and depth
                    Engine::Collision::ContactManifold manifold;
                    bool hasCollision = false;

                    if (circA && circB) {

                        // Circle-circle: single contact point (keep old method for now)
                        Vector2D n;
                        float depth;
                        const Engine::WorldCircle wcA = Engine::Physics::GetWorldCircle(*circA, tAWorld);
                        const Engine::WorldCircle wcB = Engine::Physics::GetWorldCircle(*circB, tBWorld);
                        if (TestCircleCircle(wcA, wcB, n, depth)) {
                            manifold.normal = n;
                            manifold.penetration = depth;
                            const Vector2D pA = wcA.Center + (n * wcA.Radius);
                            const Vector2D pB = wcB.Center - (n * wcB.Radius);
                            manifold.points[0] = (pA + pB) * 0.5f;
                            manifold.pointCount = 1;
                            hasCollision = true;
                        }
                    }
                    else if (boxA && boxB) {

                        // Box-box: use new manifold generation
                        const Engine::WorldOBB obbA = Engine::Physics::GetWorldOBB(*boxA, tAWorld);
                        const Engine::WorldOBB obbB = Engine::Physics::GetWorldOBB(*boxB, tBWorld);

                        // Test box-box
                        manifold = TestBoxBox(obbA, obbB);
                        hasCollision = (manifold.pointCount > 0);
                    }
                    else if (circA && boxB) {

                        // Circle-box: single contact point
                        Vector2D n;
                        float depth;
                        Vector2D contact;

                        // Use world-space shapes
                        const Engine::WorldCircle wcA = Engine::Physics::GetWorldCircle(*circA, tAWorld);
                        const Engine::WorldOBB obbB = Engine::Physics::GetWorldOBB(*boxB, tBWorld);

                        // Test circle-box
                        if (TestCircleBox(wcA, obbB, n, depth, contact)) {
                            manifold.normal = n;
                            manifold.penetration = depth;

                            manifold.points[0] = contact;
                            manifold.pointCount = 1;
                            hasCollision = true;
                        }
                    }
                    else if (boxA && circB) {

                        // Box-circle: single contact point
                        Vector2D n;
                        float depth;
                        Vector2D contact;

                        // Use world-space shapes
                        const Engine::WorldCircle wcB = Engine::Physics::GetWorldCircle(*circB, tBWorld);
                        const Engine::WorldOBB obbA = Engine::Physics::GetWorldOBB(*boxA, tAWorld);

                        // Test circle-box (flip normal later)
                        if (TestCircleBox(wcB, obbA, n, depth, contact)) {
                            manifold.normal = -n;  // Flip normal
                            manifold.penetration = depth;

                            manifold.points[0] = contact;
                            manifold.pointCount = 1;
                            hasCollision = true;
                        }
                    }

                    if (!hasCollision) continue;

                    // =====================
                    // Trigger Handling (no resolution)
                    // =====================

                    // Check for triggers on either side
                    const bool isTriggerA = (circA && (circA->Flags & 0x1u)) || (boxA && (boxA->Flags & 0x1u));
                    const bool isTriggerB = (circB && (circB->Flags & 0x1u)) || (boxB && (boxB->Flags & 0x1u));

                    // Handle trigger overlaps (no resolution)
                    if (isTriggerA || isTriggerB) {
                        if (isTriggerA) {

                            // Store trigger overlap pair (A is trigger)
                            const PackedEntityPair key = MakeTriggerPair(
                                ECS::EntityUtils::Pack(A),
                                ECS::EntityUtils::Pack(B));
                            currentTriggerOverlaps.insert(key);
                        }
                        if (isTriggerB) {

                            // Store trigger overlap pair (B is trigger)
                            const PackedEntityPair key = MakeTriggerPair(
                                ECS::EntityUtils::Pack(B),
                                ECS::EntityUtils::Pack(A));
                            currentTriggerOverlaps.insert(key);
                        }
                        continue;
                    }

                    // =====================
                    // Constraint Resolution
                    // =====================

                    // Require at least one side to be physically simulated (has rb + velocity)
                    const bool hasPhysA = world.TryGet<Components::Rigidbody2D>(A) && world.TryGet<Components::LinearVelocity2D>(A);
                    const bool hasPhysB = world.TryGet<Components::Rigidbody2D>(B) && world.TryGet<Components::LinearVelocity2D>(B);
                    if (!hasPhysA && !hasPhysB)
                    {
                        if (!loggedMissingPhysicsPair)
                        {
                            loggedMissingPhysicsPair = true;
                            LOG_WARNING("PhysicsSystem: Skipping collision event (no Rigidbody2D+LinearVelocity2D pair found).");
                        }
                        continue;
                    }

                    const PackedEntityPair pairID = MakeCollisionPair(
                        ECS::EntityUtils::Pack(A),
                        ECS::EntityUtils::Pack(B)
                    );

                    const bool firstSeenThisFrame = frameCollisions.insert(pairID).second;
                    const bool isNewCollision = (m_previousCollisions.find(pairID) == m_previousCollisions.end());

                    // Fire collision events once per frame for new pairs
                    if (firstSeenThisFrame && isNewCollision) {
                        eventDispatcher.FireCollisionEvent(
                            ECS::EntityUtils::Pack(A), ECS::EntityUtils::Pack(B),
                            Vector3D(manifold.points[0].X, manifold.points[0].Y, 0.0f),
                            Vector3D(manifold.normal.X, manifold.normal.Y, 0.0f),
                            Vector3D(0.0f, 0.0f, 0.0f), 
                            0.0f  
                        );
                        collisionEnterCount++;
                    }

                    // Gather physics state (by value) and current velocities; some may be missing
                    Components::Rigidbody2D      rbA{ 0 }, rbB{ 0 };
                    Components::LinearVelocity2D vA{ {0,0} }, vB{ {0,0} };

                    // Read component pointers; if present, copy their values into locals
                    const auto* rbAp = world.TryGet<Components::Rigidbody2D>(A);
                    const auto* rbBp = world.TryGet<Components::Rigidbody2D>(B);
                    auto* vAp = world.TryGet<Components::LinearVelocity2D>(A);
                    auto* vBp = world.TryGet<Components::LinearVelocity2D>(B);
                    if (rbAp) rbA = *rbAp; if (rbBp) rbB = *rbBp;
                    if (vAp)  vA = *vAp;  if (vBp)  vB = *vBp;

                    // Fetch materials (friction, restitution, position-correction factor)
                    // Use sensible defaults if an entity has no material component
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

                    // Resolve velocity + positional correction for this manifold
                    Engine::Physics::ResolveCollisionManifold(rbA, rbB, vA, vB, *tA, *tB, manifold, mCombined);

                    // Write back
                    if (vAp) *vAp = vA;
                    if (vBp) *vBp = vB;
                    refreshWorldTransformCache(A);
                    refreshWorldTransformCache(B);

                    ++resolved;
                    {

                        // Compute impact magnitude from relative velocity along normal before the next iteration changes it further
                        const Vector2D rel = vB.Value - vA.Value;
                        const float vn = rel.X * manifold.normal.X + rel.Y * manifold.normal.Y;
                        const float    impactSpeed = std::abs(vn);

                        // Filter tiny contacts to avoid spam; tune as needed
                        constexpr float kImpactThreshold = 80.0f;

                        if (impactSpeed >= kImpactThreshold) {

                            // Per-frame dedupe for this pair
                            static uint64_t s_lastFrameSeen = 0;
                            static std::unordered_set<PackedEntityPair, PackedEntityPairHash> sfxPlayedThisFrame;
                            if (s_lastFrameSeen != s_frameCounter) {
                                sfxPlayedThisFrame.clear(); s_lastFrameSeen = s_frameCounter;
                            }
                            const PackedEntityPair pk = MakeCollisionPair(
                                ECS::EntityUtils::Pack(A),
                                ECS::EntityUtils::Pack(B)
                            );

                            if (sfxPlayedThisFrame.insert(pk).second) {
                                if (auto* audio = PHYSICS_AUDIO_DEVICE) {
                                    Audio::PlaySettings ps;
                                    ps.Loop = false;

                                    // Scale volume by impact; clamp to [0.2, 1.0]
                                    ps.Volume = std::max(0.2f, std::min(impactSpeed / 350.0f, 1.0f));
                                    ps.Pitch = 1.0f;
                                    audio->PlaySingle("sfx_collide", ps, Audio::PlayPolicy::SingleInstanceRestart);
                                }
                            }
                        }
                    }
                }

                if (resolved == 0) break;
            }

            // =====================
            // Tilemap Collisions (Grid Query)
            // =====================

            // For each dynamic entity, query the spatial partitioning for nearby tiles and test collisions against them
            if (!m_runtimeTileMaps.empty()) {

                // Small epsilon to prevent edge cases where an entity is exactly on the boundary between two tiles, 
                // which could cause it to miss colliding with one of them due to floating-point precision issues
                constexpr float kTileCoordEpsilon = 1e-4f;

                // Iterate all dynamic entities and check for collisions against nearby tiles in enabled tilemaps
                for (Entity e : dynamicEntities) {

                    // Skip if entity got destroyed during earlier steps
                    if (!world.IsAlive(e)) continue;
                    if (!world.IsActiveInHierarchy(e)) continue;

                    // Fetch required components; if missing, skip
                    // We need transform for position, rigidbody and velocity for physics state
                    auto* tA = world.TryGet<Components::LocalTransform>(e);
                    auto* rbAp = world.TryGet<Components::Rigidbody2D>(e);
                    auto* vAp = world.TryGet<Components::LinearVelocity2D>(e);
                    if (!tA || !rbAp || !vAp) continue;
                    if (rbAp->Mass <= 0.0f) continue; // Zero/negative mass = static or invalid; skip

                    // Query colliders; if no collider, skip (nothing to collide with tiles)
                    const auto* circA = world.TryGet<Components::CircleCollider2D>(e);
                    const auto* boxA = world.TryGet<Components::BoxCollider2D>(e);
                    if (!circA && !boxA) continue;

                    // Check if this entity is a trigger; if so, skip tilemap collision (triggers don't resolve)
                    // Trigger flag is stored in bit 0 of the collider's Flags field
                    const bool isTriggerA = (circA && (circA->Flags & 0x1u)) || (boxA && (boxA->Flags & 0x1u));
                    if (isTriggerA) continue;

                    // Layer mask check: if entity is not in a layer that collides with tilemap layers, skip
                    const auto* la = world.TryGet<Components::Layer>(e);
                    if (!la) {
                        if (!loggedMissingLayer) {
                            loggedMissingLayer = true;
                            LOG_WARNING("PhysicsSystem: Skipping collision event (missing Layer component on one or both entities).");
                        }
                        continue;
                    }

                    // Get entity's layer ID and its collision mask (bitmask of which layers it collides with)
                    const uint16_t layerAId = la->Id;
                    const uint32_t maskA = layerManager->GetLayerMask(layerAId);

                    // Resolve entity's physics material, falling back to defaults if none assigned
                    Components::PhysicsMaterial2D mA{ 0.2f, 0.5f, 0.5f };
                    if (const auto* mpA = world.TryGet<Components::PhysicsMaterial2D>(e)) {
                        mA = *mpA;
                    }

                    // Tilemap uses a fixed default material (no per-tile material support yet)
                    Components::PhysicsMaterial2D mB{ 0.2f, 0.5f, 0.5f };

                    // Combine entity and tilemap materials into a single interaction material:
                    // * Friction: average of both (blends surface properties)
                    // * Restitution: max of both (bouncier surface wins, physically plausible)
                    // * PositionCorrectPercent: average of both (balanced position correction)
                    const Components::PhysicsMaterial2D mCombined{
                        (mA.Friction + mB.Friction) * 0.5f,
                        std::max(mA.Restitution, mB.Restitution),
                        (mA.PositionCorrectPercent + mB.PositionCorrectPercent) * 0.5f
                    };

                    Components::LocalTransform tAWorld{};
                    Engine::WorldCircle worldCircle{};
                    Engine::WorldOBB worldObb{};
                    Engine::WorldAABB worldAABB{};
                    auto refreshEntityWorldShape = [&]() -> bool {
                        if (!getWorldTransformCached(e, tAWorld)) {
                            return false;
                        }

                        if (circA) {
                            worldCircle = Engine::Physics::GetWorldCircle(*circA, tAWorld);
                            worldAABB.Center = worldCircle.Center;
                            worldAABB.HalfExtents = Vector2D(worldCircle.Radius, worldCircle.Radius);
                        }
                        else {
                            worldObb = Engine::Physics::GetWorldOBB(*boxA, tAWorld);
                            worldAABB = Engine::Physics::GetWorldAABB(*boxA, tAWorld);
                        }
                        return true;
                    };
                    if (!refreshEntityWorldShape()) continue;

                    // Check collisions against all enabled tilemaps that collide with this entity's layer
                    for (const auto& entryPair : m_runtimeTileMaps) {
                        const RuntimeTileMapEntry& entry = entryPair.second;

                        // Skip disabled tilemaps or ones with no layers (nothing to collide against)
                        if (!entry.Enabled || !entry.Map) continue;
                        if (entry.Map->LayerCount() == 0) continue;

                        // Skip tilemaps whose layer has physics disabled entirely
                        const auto& layerData = layerManager->Get(entry.LayerId);
                        if (!layerData.physicsEnabled) continue;

                        // Check if this entity's layer and the tilemap's layer are configured to collide with each other
                        const uint32_t maskB = layerManager->GetLayerMask(entry.LayerId);
                        if (!Engine::CanCollide(maskA, layerAId, maskB, entry.LayerId)) {
                            continue;
                        }

                        // Get tile size from the tilemap; a non-positive tile size is invalid so skip
                        const float tileSize = entry.Map->TileSize();
                        if (tileSize <= 0.0f) continue;

                        // Compute the world-space AABB of the entity's collider to find which tiles it overlaps with
                        // We query tiles in this range rather than every tile in the map for efficiency

                        // Compute the world-space min/max corners of the entity's AABB
                        const float minX = worldAABB.Center.X - worldAABB.HalfExtents.X;
                        const float maxX = worldAABB.Center.X + worldAABB.HalfExtents.X;
                        const float minY = worldAABB.Center.Y - worldAABB.HalfExtents.Y;
                        const float maxY = worldAABB.Center.Y + worldAABB.HalfExtents.Y;

                        // Transform world-space AABB corners into the tilemap's local space by subtracting the tilemap's world origin
                        // Apply epsilon to the max edge to avoid sampling one tile too many on exact boundary cases
                        const float localMinX = minX - entry.Origin.X;
                        const float localMinY = minY - entry.Origin.Y;
                        const float localMaxX = maxX - entry.Origin.X - kTileCoordEpsilon;
                        const float localMaxY = maxY - entry.Origin.Y - kTileCoordEpsilon;

                        // Convert local coordinates to integer tile indices
                        // WorldToTileSigned handles negative coordinates (tiles to the left/below origin)
                        int32_t tileMinX = entry.Map->WorldToTileSigned(localMinX);
                        int32_t tileMinY = entry.Map->WorldToTileSigned(localMinY);
                        int32_t tileMaxX = entry.Map->WorldToTileSigned(localMaxX);
                        int32_t tileMaxY = entry.Map->WorldToTileSigned(localMaxY);

                        // Ensure min <= max in case the tilemap's coordinate space is flipped
                        if (tileMaxX < tileMinX) std::swap(tileMaxX, tileMinX);
                        if (tileMaxY < tileMinY) std::swap(tileMaxY, tileMinY);

                        // Precompute tile geometry constants used across all tiles in this tilemap:
                        // * tileHalf: half the tile size ? center offset and full-tile half-extents
                        // * subHalf: quarter of the tile size ? used for sub-tile collision cells (half-tile quadrants)
                        // * subHalfExtents: half-extents for a single quadrant collision cell
                        const float tileHalf = tileSize * 0.5f;
                        const float subHalf = tileSize * 0.25f;
                        const Vector2D subHalfExtents(subHalf, subHalf);

                        // Iterate over every tile in the AABB's tile range
                        for (int32_t ty = tileMinY; ty <= tileMaxY; ++ty) {
                            for (int32_t tx = tileMinX; tx <= tileMaxX; ++tx) {

                                // Skip empty tiles; no geometry to collide against
                                if (entry.Map->GetTileSigned(0, tx, ty) == EMPTY_TILE) {
                                    continue;
                                }

                                // Get the 4-bit collision mask for this tile
                                // Each bit represents one quadrant: TopLeft, TopRight, BottomLeft, BottomRight
                                // A mask of 0 means the tile exists visually but has no collision
                                const uint8_t mask = static_cast<uint8_t>(entry.Map->GetCollisionMaskSigned(tx, ty) & 0x0F);
                                if (mask == 0) {
                                    continue;
                                }

                                // Compute the world-space origin (bottom-left corner) of this tile
                                const float tileWorldX = entry.Origin.X + entry.Map->TileToWorldSigned(tx);
                                const float tileWorldY = entry.Origin.Y + entry.Map->TileToWorldSigned(ty);

                                // resolveCell: tests and resolves a collision between the entity and a rectangular sub-cell
                                // within the current tile
                                // The cell is defined by its center offset from the tile origin and its half-extents
                                // This allows partial-tile collision shapes (quadrants, half-tiles, etc.)
                                auto resolveCell = [&](float centerOffsetX, float centerOffsetY, const Vector2D& halfExtents) {
                                    const Vector2D cellCenter(tileWorldX + centerOffsetX, tileWorldY + centerOffsetY);
                                    Engine::Collision::ContactManifold manifold;

                                    if (circA) {

                                        // Circle vs. tile cell OBB test
                                        // Build an axis-aligned OBB for the cell (rotation = 0, standard axes)
                                        Engine::WorldOBB tileObb;
                                        tileObb.Center = cellCenter;
                                        tileObb.HalfExtents = halfExtents;
                                        tileObb.Rotation = 0.0f;
                                        tileObb.AxisX = Vector2D(1.0f, 0.0f);
                                        tileObb.AxisY = Vector2D(0.0f, 1.0f);

                                        Vector2D n;
                                        float depth = 0.0f;
                                        Vector2D contact;
                                        if (!TestCircleBox(worldCircle, tileObb, n, depth, contact)) {
                                            return; // No overlap, nothing to resolve
                                        }

                                        // Pack result into manifold for unified resolution below
                                        manifold.normal = n;
                                        manifold.penetration = depth;
                                        manifold.points[0] = contact;
                                        manifold.pointCount = 1;
                                    }
                                    else {

                                        // Box vs. tile cell OBB test (entity box may be rotated; tile cell is always axis-aligned)
                                        Engine::WorldOBB tileObb;
                                        tileObb.Center = cellCenter;
                                        tileObb.HalfExtents = halfExtents;
                                        tileObb.Rotation = 0.0f;
                                        tileObb.AxisX = Vector2D(1.0f, 0.0f);
                                        tileObb.AxisY = Vector2D(0.0f, 1.0f);

                                        manifold = TestBoxBox(worldObb, tileObb);
                                        if (manifold.pointCount <= 0) {
                                            return; // No overlap, nothing to resolve
                                        }
                                    }

                                    // Construct a synthetic static rigidbody for the tile cell
                                    // Mass = 0 signals to the resolver that this body is immovable (infinite mass)
                                    Components::Rigidbody2D rbB{};
                                    rbB.Mass = 0.0f;
                                    Components::LinearVelocity2D vB{ {0.0f, 0.0f} };
                                    Components::LocalTransform tB{};
                                    tB.Position = { cellCenter.X, cellCenter.Y, 0.0f };
                                    tB.Scale = { 1.0f, 1.0f, 1.0f };
                                    tB.Rotation = Quaternion::Identity();

                                    // Apply impulse-based collision resolution and positional correction
                                    Engine::Physics::ResolveCollisionManifold(
                                        *rbAp, rbB, *vAp, vB, *tA, tB, manifold, mCombined);
                                    refreshWorldTransformCache(e);
                                    if (!refreshEntityWorldShape()) {
                                        return;
                                    }
                                    };

                                // Dispatch collision cells based on the tile's collision mask
                                // Masks define which portions of the tile are solid
                                // We map them to axis-aligned rectangular cells and resolve each independently
                                // Full tile solid (all 4 quadrants); treat as one full-size box
                                if (mask == 0x0F) {
                                    resolveCell(tileHalf, tileHalf, Vector2D(tileHalf, tileHalf));
                                    continue;
                                }

                                // Half-tile combinations: merge two quadrants into one wider/taller box
                                // to avoid resolving two adjacent cells with a seam between them
                                // Bottom half (BL + BR): full width, bottom half height
                                if (mask == (kCollisionMaskBottomLeft | kCollisionMaskBottomRight)) {
                                    resolveCell(tileHalf, subHalf, Vector2D(tileHalf, subHalf));
                                    continue;
                                }

                                // Top half (TL + TR): full width, top half height
                                if (mask == (kCollisionMaskTopLeft | kCollisionMaskTopRight)) {
                                    resolveCell(tileHalf, tileSize - subHalf, Vector2D(tileHalf, subHalf));
                                    continue;
                                }

                                // Left half (BL + TL): left half width, full height
                                if (mask == (kCollisionMaskBottomLeft | kCollisionMaskTopLeft)) {
                                    resolveCell(subHalf, tileHalf, Vector2D(subHalf, tileHalf));
                                    continue;
                                }

                                // Right half (BR + TR): right half width, full height
                                if (mask == (kCollisionMaskBottomRight | kCollisionMaskTopRight)) {
                                    resolveCell(tileSize - subHalf, tileHalf, Vector2D(subHalf, tileHalf));
                                    continue;
                                }

                                // Individual quadrant cells: resolve each set bit as its own quarter-tile box
                                // These handle diagonal/corner-only collision shapes
                                if (mask & kCollisionMaskBottomLeft) {
                                    resolveCell(subHalf, subHalf, subHalfExtents);
                                }
                                if (mask & kCollisionMaskBottomRight) {
                                    resolveCell(tileSize - subHalf, subHalf, subHalfExtents);
                                }
                                if (mask & kCollisionMaskTopLeft) {
                                    resolveCell(subHalf, tileSize - subHalf, subHalfExtents);
                                }
                                if (mask & kCollisionMaskTopRight) {
                                    resolveCell(tileSize - subHalf, tileSize - subHalf, subHalfExtents);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Emit collision exits once per frame (after all substeps)
        for (const PackedEntityPair& pairID : m_previousCollisions) {
            if (frameCollisions.find(pairID) == frameCollisions.end()) {
                Entity entityA = ECS::EntityUtils::Unpack(pairID.A);
                Entity entityB = ECS::EntityUtils::Unpack(pairID.B);

                if (world.IsAlive(entityA) && world.IsAlive(entityB)) {
                    eventDispatcher.FireCollisionExitEvent(
                        ECS::EntityUtils::Pack(entityA), ECS::EntityUtils::Pack(entityB),
                        Vector3D(0.0f, 0.0f, 0.0f)  // TODO: Track last contact point
                    );
                    collisionExitCount++;
                }
            }
        }

        m_previousCollisions = std::move(frameCollisions);

        // Handle trigger events
        for (const PackedEntityPair& pairID : currentTriggerOverlaps) {

            // Check if was overlapping last frame
            const bool wasOverlapping = (m_previousTriggerOverlaps.find(pairID) != m_previousTriggerOverlaps.end());
            const PackedEntityId triggerId = pairID.A;
            const PackedEntityId otherId = pairID.B;

            // Resolve entities
            Entity triggerEntity = ECS::EntityUtils::Unpack(triggerId);
            Entity otherEntity = ECS::EntityUtils::Unpack(otherId);

            // Check if both entities are still alive
            if (world.IsAlive(triggerEntity) && world.IsAlive(otherEntity)) {
                if (wasOverlapping) {
                    eventDispatcher.FireTriggerStayEvent(ECS::EntityUtils::Pack(triggerEntity), ECS::EntityUtils::Pack(otherEntity));
                    triggerStayCount++;
                }
                else {
                    eventDispatcher.FireTriggerEnterEvent(ECS::EntityUtils::Pack(triggerEntity), ECS::EntityUtils::Pack(otherEntity));
                    triggerEnterCount++;
                }
            }
        }

        // Check for ended trigger overlaps
        for (const PackedEntityPair& pairID : m_previousTriggerOverlaps) {
            if (currentTriggerOverlaps.find(pairID) == currentTriggerOverlaps.end()) {
                const PackedEntityId triggerId = pairID.A;
                const PackedEntityId otherId = pairID.B;

                Entity triggerEntity = ECS::EntityUtils::Unpack(triggerId);
                Entity otherEntity = ECS::EntityUtils::Unpack(otherId);

                if (world.IsAlive(triggerEntity) && world.IsAlive(otherEntity)) {
                    eventDispatcher.FireTriggerExitEvent(ECS::EntityUtils::Pack(triggerEntity), ECS::EntityUtils::Pack(otherEntity));
                    triggerExitCount++;
                }
            }
        }

        m_previousTriggerOverlaps = std::move(currentTriggerOverlaps);
        }
    }
}  // namespace ECS
