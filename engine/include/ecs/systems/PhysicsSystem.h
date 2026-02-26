/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file PhysicsSystem.h
 * @brief Broad/narrow-phase utilities and per-frame 2D physics update loop.
 *
 * @details
 * Might be edited later on if required to change structure of how physicSystem 
 * and a more foolproof version of broad-narrow phase collision is implemented 
 * with more specific shape handling or better optimisation.
 */
#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include "ecs/World.h"
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

// Forward declaration
class TileMap;

namespace ECS {
    struct PackedEntityPair {
        PackedEntityId A{};
        PackedEntityId B{};

        bool operator==(const PackedEntityPair& other) const {
            return A == other.A && B == other.B;
        }
    };

    struct PackedEntityPairHash {
        size_t operator()(const PackedEntityPair& pair) const noexcept {
            const size_t h1 = std::hash<uint64_t>{}(pair.A);
            const size_t h2 = std::hash<uint64_t>{}(pair.B);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

    /**
     * @brief System that handles 2D physics simulation with broad/narrow-phase collision
     * Executes in Physics phase with executionOrder=0
     */
    class PhysicsSystem : public ISystem {
    public:
        PhysicsSystem() = default;
        ~PhysicsSystem() override = default;

        // ISystem interface
        void OnCreate(World& world) override {}
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Physics; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

    private:
		// Helper struct to track runtime tilemap data for physics synchronization
        struct RuntimeTileMapEntry {
			std::shared_ptr<TileMap> Map;  // Pointer to the active tilemap asset
			std::string MapPath;           // Tileset paths are stored in the tilemap asset, but we cache them here for quick access during physics updates
            float TileWorldSize = 1.0f;
            uint32_t DefaultWidth = 0;
            uint32_t DefaultHeight = 0;
            Vector2D Origin{0.0f, 0.0f};
            uint16_t LayerId = 0;
            bool Enabled = false;
			uint32_t Generation = 0;       // Used to track changes to the tilemap for efficient updates
        };

		// Refreshes the runtime tilemap cache by comparing current tilemap components in the world to the cached entries
        void RefreshRuntimeTileMaps(World& world);

		// Performs broad-phase collision detection using spatial partitioning (e.g. uniform grid) to find potential collision pairs
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> m_previousCollisions;
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> m_previousTriggerOverlaps;

		// Cache of runtime tilemap data for physics synchronization, keyed by entity ID
        std::unordered_map<EntityId, RuntimeTileMapEntry> m_runtimeTileMaps;  
    };
}

#endif
