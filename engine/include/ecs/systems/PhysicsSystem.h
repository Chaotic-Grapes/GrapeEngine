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
    /**
     * @brief Canonical packed entity-pair key used for collision/trigger tracking.
     */
    struct PackedEntityPair {
        PackedEntityId A{};
        PackedEntityId B{};

        /**
         * @brief Compare pair keys for equality.
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
         */
        void OnCreate(World& world) override { (void)world; }

        /**
         * @brief Execute physics update for the current frame.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief Release runtime references and caches.
         */
        void OnDestroy(World& world) override;

        /**
         * @brief Describe scheduler component access and run policy.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Return scheduler system group.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::Physics; }

        /**
         * @brief Restrict system execution to play mode.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

        /**
         * @brief Ray cast against current runtime body cache.
         */
        Engine::Physics2D::RayCastHit2D RayCast(const Engine::Physics2D::RayCastInput2D& input) const;

        /**
         * @brief Query entities whose AABBs overlap the supplied AABB.
         */
        std::vector<Entity> OverlapAABB(const Engine::WorldAABB& query) const;

        /**
         * @brief Query entities whose AABBs contain the supplied point.
         */
        std::vector<Entity> QueryPoint(const Vector2D& point) const;

        /**
         * @brief Return latest physics performance and workload stats.
         */
        const Engine::Physics2D::PhysicsStats2D& GetStats() const { return m_physicsWorld2D.GetStats(); }

        /**
         * @brief Return unconsumed fixed-step accumulator time for render interpolation.
         */
        float GetRemainingAccumulatorSeconds() const { return m_accumulatorSeconds; }

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

    private:
        friend class PhysicsPipelineRunner;

        /**
         * @brief Reconcile runtime tilemap cache with ECS tilemap components.
         */
        void RefreshRuntimeTileMaps(World& world);

        /**
         * @brief Convert runtime tilemap cache into PhysicsWorld2D proxy payloads.
         */
        std::vector<Engine::Physics2D::TilemapCollisionProxy2D> BuildTilemapProxies() const;

        // Stores collision pairs from previous frame to drive enter and exit event transitions
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> m_previousCollisions;

        // Stores trigger overlaps from previous frame for trigger enter and exit tracking
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> m_previousTriggerOverlaps;

        // Runtime tilemap cache keyed by entity id for fast physics queries and sync checks
        std::unordered_map<EntityId, RuntimeTileMapEntry> m_runtimeTileMaps;

        // Fixed-step coordinator state.
        Engine::Physics2D::PhysicsWorld2D m_physicsWorld2D;
        float m_accumulatorSeconds = 0.0f;
        const World* m_lastWorld = nullptr;
    };
}

#endif  // PHYSICS2D_H
