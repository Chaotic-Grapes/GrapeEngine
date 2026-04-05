/* Start Header *****************************************************************/
/*!
\file   PhysicsSystem.h
\author Dalton Koh (95%)
        Foo Rui Qin (5%)
\par    d.koh@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
Declares the 2D physics system and supporting data structures used for
broad-phase pairing, overlap tracking and runtime tilemap synchronization.
*
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include "ecs/World.h"
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
#include "physics2d/PhysicsWorld2D.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

// Forward declaration
class TileMap;

namespace ECS {
    class PhysicsPipelineRunner;

    /**
     * @brief Canonical packed entity-pair key used for collision/trigger tracking.
     */
    struct PackedEntityPair {
        PackedEntityId A{};
        PackedEntityId B{};

        /**
         * @brief Compare pair keys for equality.
         * @param other Pair to compare against.
         * @return True when both A and B fields match.
         */
        bool operator==(const PackedEntityPair& other) const {
            return A == other.A && B == other.B;
        }
    };

    /**
     * @brief Hash functor for PackedEntityPair.
     */
    struct PackedEntityPairHash {
        /**
         * @brief Compute hash code for a packed pair key.
         * @param pair Pair to hash.
         * @return Combined hash of the two packed entity ids.
         */
        size_t operator()(const PackedEntityPair& pair) const noexcept {
            const size_t h1 = std::hash<uint64_t>{}(pair.A);
            const size_t h2 = std::hash<uint64_t>{}(pair.B);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

    // Handles 2D physics simulation using broad and narrow phase collision workflow
    class PhysicsSystem : public ISystem {
    public:
        struct RuntimeTileMapEntry;

        /**
         * @brief Construct system with default-initialized runtime caches.
         */
        PhysicsSystem() = default;

        /**
         * @brief Default destructor.
         */
        ~PhysicsSystem() override = default;

        // ISystem interface

        /**
         * @brief Initialize system state.
         * @param world ECS world passed by the scheduler (unused).
         */
        void OnCreate(World& world) override { (void)world; }

        /**
         * @brief Execute physics update for the current frame.
         * @param world ECS world containing entities with physics components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief Release runtime references and caches.
         * @param world ECS world passed by the scheduler.
         */
        void OnDestroy(World& world) override;

        /**
         * @brief Describe scheduler component access and run policy.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Return the scheduler system group.
         * @return SystemGroup::Physics.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::Physics; }

        /**
         * @brief Restrict system execution to play mode.
         * @return SystemRunMode::PlayOnly.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

        /**
         * @brief Ray cast against current runtime body cache.
         * @param input Ray origin, direction, and max distance.
         * @return Hit result including entity, point, normal, and fraction.
         */
        Engine::Physics2D::RayCastHit2D RayCast(const Engine::Physics2D::RayCastInput2D& input) const;

        /**
         * @brief Query entities whose AABBs overlap the supplied AABB.
         * @param query AABB to test against all broadphase bodies.
         * @return List of overlapping entities.
         */
        std::vector<Entity> OverlapAABB(const Engine::WorldAABB& query) const;

        /**
         * @brief Query entities whose AABBs contain the supplied point.
         * @param point World-space point to test.
         * @return List of entities whose AABB contains the point.
         */
        std::vector<Entity> QueryPoint(const Vector2D& point) const;

        /**
         * @brief Return latest physics performance and workload stats.
         * @return Reference to the current frame's physics statistics.
         */
        const Engine::Physics2D::PhysicsStats2D& GetStats() const { return m_physicsWorld2D.GetStats(); }

        /**
         * @brief Return unconsumed fixed-step accumulator time for render interpolation.
         * @return Remaining accumulator time in seconds.
         */
        float GetRemainingAccumulatorSeconds() const { return m_accumulatorSeconds; }

        /**
         * @brief Internal bridge for legacy staged pipeline to refresh the tilemap runtime cache.
         * @param world ECS world containing tilemap component data.
         */
        void RefreshRuntimeTileMapsForPipeline(World& world) { RefreshRuntimeTileMaps(world); }

        /**
         * @brief Internal bridge exposing the tilemap runtime cache for legacy staged pipeline.
         * @return Reference to the map from entity id to RuntimeTileMapEntry.
         */
        const std::unordered_map<EntityId, RuntimeTileMapEntry>& GetRuntimeTileMapsForPipeline() const { return m_runtimeTileMaps; }

        /**
         * @brief Cached tilemap state mirrored from ECS for physics stepping.
         */
        struct RuntimeTileMapEntry {
            std::shared_ptr<TileMap> Map;  // Pointer to active tilemap asset used during physics step
            std::string MapPath;           // Cached map asset path for quick lookup and change tracking
            float TileWorldSize = 1.0f;
            uint32_t DefaultWidth = 0;
            uint32_t DefaultHeight = 0;
            Vector2D Origin{0.0f, 0.0f};
            uint16_t LayerId = 0;
            bool Enabled = false;
            uint32_t Generation = 0;       // Tracks tilemap revision so updates can skip unchanged data
        };

    public:
        /**
         * @brief Reconcile the runtime tilemap cache with ECS tilemap components.
         *        Internal: exposed for legacy staged physics runner.
         * @param world ECS world to read tilemap components from.
         */
        void RefreshRuntimeTileMaps(World& world);

    private:

        /**
         * @brief Convert the runtime tilemap cache into PhysicsWorld2D proxy payloads.
         * @return Vector of TilemapCollisionProxy2D built from the current cache.
         */
        std::vector<Engine::Physics2D::TilemapCollisionProxy2D> BuildTilemapProxies() const;

    public:
        // Runtime tilemap cache keyed by entity id for fast physics queries and sync checks.
        // Internal: exposed for legacy staged physics runner.
        std::unordered_map<EntityId, RuntimeTileMapEntry> m_runtimeTileMaps;

    private:

        // Fixed-step coordinator state.
        Engine::Physics2D::PhysicsWorld2D m_physicsWorld2D;
        float m_accumulatorSeconds = 0.0f;
        const World* m_lastWorld = nullptr;
    };
}

#endif  // PHYSICS2D_H
