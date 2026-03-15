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
#include "ecs/systems/internal/PhysicsPipelineRunner.h"
#include "services/TimeSystem.h"
#include "physics/Physics.h"
#include "ecs/Components.h"
#include "core/World/TileMap.hpp"
#include "core/ProjectPaths.h"
#include "ecs/StringTable.h"
#include <unordered_set>
#include <filesystem>
#include "helpers/TransformUtils.h"
#include "math/Vector3D.h"

namespace {
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

    void PhysicsSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        if (!Engine::Physics::IsEnabled() || dt <= 0.0f) {
            return;
        }

        auto* layerManager = world.GetLayerManager();
        if (!layerManager) {
            return;
        }

        RunPhysicsPipeline(*this, world, *layerManager, dt);
    }
}  // namespace ECS
