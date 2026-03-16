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
#include "physics/Physics.h"
#include "ecs/Components.h"
#include "core/World/TileMap.hpp"
#include "core/ProjectPaths.h"
#include "ecs/StringTable.h"
#include <unordered_set>
#include <filesystem>
#include <algorithm>
#include "helpers/TransformUtils.h"
#include "math/Vector3D.h"
#include "ecs/events/EventDispatcher.h"
#include "physics2d/PhysicsQueries2D.h"

namespace {
    /**
     * @brief Resolve a project-relative asset path into an absolute normalized path.
     * @param path Relative or absolute input path.
     * @return Absolute normalized path when project paths are initialized; otherwise original path.
     */
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

    /**
     * @brief Resolve effective transform data for a tilemap entity.
     * @param world ECS world.
     * @param entity Tilemap entity.
     * @param lt Fallback local transform.
     * @param outPosition Output world position.
     * @param outRotation Output world rotation.
     * @param outScale Output world scale.
     */
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
    /**
     * @brief Build scheduler metadata for PhysicsSystem.
     * @return Component access and run configuration.
     */
    SystemMetadata PhysicsSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Physics");

        // Read accesses
        builder.ReadComponent<Components::LocalTransform>();
        builder.ReadComponent<Components::CircleCollider2D>();
        builder.ReadComponent<Components::BoxCollider2D>();
        builder.ReadComponent<Components::Rigidbody2D>();
        builder.ReadComponent<Components::LinearVelocity2D>();
        builder.ReadComponent<Components::AngularVelocity2D>();
        builder.ReadComponent<Components::PhysicsMaterial2D>();
        builder.ReadComponent<Components::Layer>();
        builder.ReadComponent<Components::Active>();
        builder.ReadComponent<Components::Parent>();

        // Write accesses
        builder.WriteComponent<Components::LocalTransform>();
        builder.WriteComponent<Components::Rigidbody2D>();
        builder.WriteComponent<Components::LinearVelocity2D>();
        builder.WriteComponent<Components::AngularVelocity2D>();
        builder.WriteComponent<Components::PhysicsBodyHandle2D>();
        builder.WriteComponent<Components::PhysicsActiveTag>();
        builder.WriteComponent<Components::SleepingTag>();
        builder.WriteComponent<Components::PhysicsContactCache2D>();
        builder.WriteComponent<ECS::Events::CollisionEventBuffer>();
        builder.WriteComponent<ECS::Events::TriggerEventBuffer>();
        builder.WriteComponent<ECS::Events::CollisionExitEventBuffer>();
        builder.WriteComponent<ECS::Events::TriggerExitEventBuffer>();

        // Execution parameters
        builder.SetExecutionOrder(0);
        builder.SetGroup(SystemGroup::Physics);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    /**
     * @brief Clear runtime state when the system is destroyed.
     * @param world ECS world (unused).
     */
    void PhysicsSystem::OnDestroy(World& /*world*/) {
        m_previousCollisions.clear();
        m_previousTriggerOverlaps.clear();
        m_runtimeTileMaps.clear();
        m_accumulatorSeconds = 0.0f;
        m_lastWorld = nullptr;
        m_physicsWorld2D = Engine::Physics2D::PhysicsWorld2D{};
    }

    /**
     * @brief Refresh tilemap runtime cache from ECS components.
     * @param world ECS world.
     */
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

    /**
     * @brief Advance fixed-step 2D physics simulation.
     * @param world ECS world.
     */
    void PhysicsSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        if (!Engine::Physics::IsEnabled() || dt <= 0.0f) {
            return;
        }

        // Reset cached runtime/contact state when the active world changes.
        if (m_lastWorld != &world) {
            m_lastWorld = &world;
            m_runtimeTileMaps.clear();
            m_previousCollisions.clear();
            m_previousTriggerOverlaps.clear();
            m_accumulatorSeconds = 0.0f;
            m_physicsWorld2D = Engine::Physics2D::PhysicsWorld2D{};
        }

        auto* layerManager = world.GetLayerManager();
        if (!layerManager) {
            return;
        }

        RefreshRuntimeTileMaps(world);
        m_physicsWorld2D.Config().FixedDeltaSeconds = static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
        m_accumulatorSeconds += dt;
        uint32_t steps = 0;
        const uint32_t maxSteps = m_physicsWorld2D.Config().MaxFixedStepsPerFrame;
        const float fixed = m_physicsWorld2D.Config().FixedDeltaSeconds;
        ECS::Events::EventDispatcher dispatcher(&world);
        const auto tileProxies = BuildTilemapProxies();

        while (m_accumulatorSeconds >= fixed && steps < maxSteps) {
            m_physicsWorld2D.StepFixed(world, *layerManager, dispatcher, fixed, tileProxies);
            m_accumulatorSeconds -= fixed;
            ++steps;
        }
    }

    /**
     * @brief Build sorted tilemap collision proxies from runtime cache.
     * @return Deterministically ordered tilemap proxies.
     */
    std::vector<Engine::Physics2D::TilemapCollisionProxy2D> PhysicsSystem::BuildTilemapProxies() const {
        std::vector<Engine::Physics2D::TilemapCollisionProxy2D> proxies;
        proxies.reserve(m_runtimeTileMaps.size());
        for (const auto& [entityId, entry] : m_runtimeTileMaps) {
            (void)entityId;
            Engine::Physics2D::TilemapCollisionProxy2D p{};
            p.Map = entry.Map;
            p.Origin = entry.Origin;
            p.LayerId = entry.LayerId;
            p.Enabled = entry.Enabled;
            proxies.push_back(std::move(p));
        }
        std::sort(proxies.begin(), proxies.end(), [](const auto& a, const auto& b) {
            if (a.LayerId != b.LayerId) {
                return a.LayerId < b.LayerId;
            }
            if (a.Origin.X != b.Origin.X) {
                return a.Origin.X < b.Origin.X;
            }
            return a.Origin.Y < b.Origin.Y;
            });
        return proxies;
    }

    /**
     * @brief Ray cast against the current physics runtime body cache.
     * @param input Ray input.
     * @return Nearest ray hit result.
     */
    Engine::Physics2D::RayCastHit2D PhysicsSystem::RayCast(const Engine::Physics2D::RayCastInput2D& input) const {
        return Engine::Physics2D::PhysicsQueries2D::RayCast(m_physicsWorld2D.Bodies(), input);
    }

    /**
     * @brief Query entities overlapping an AABB.
     * @param query Query AABB.
     * @return Matching entities.
     */
    std::vector<Entity> PhysicsSystem::OverlapAABB(const Engine::WorldAABB& query) const {
        return Engine::Physics2D::PhysicsQueries2D::OverlapAABB(m_physicsWorld2D.Bodies(), query);
    }

    /**
     * @brief Query entities containing a point.
     * @param point World-space query point.
     * @return Matching entities.
     */
    std::vector<Entity> PhysicsSystem::QueryPoint(const Vector2D& point) const {
        return Engine::Physics2D::PhysicsQueries2D::QueryPoint(m_physicsWorld2D.Bodies(), point);
    }
}  // namespace ECS
