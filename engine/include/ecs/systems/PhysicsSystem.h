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

        // Compares packed pairs by value so unordered containers can detect exact pair identity
        bool operator==(const PackedEntityPair& other) const {
            return A == other.A && B == other.B;
        }
    };

    struct PackedEntityPairHash {
        // Hashes packed pair fields and mixes them to reduce clustering in unordered containers
        size_t operator()(const PackedEntityPair& pair) const noexcept {
            const size_t h1 = std::hash<uint64_t>{}(pair.A);
            const size_t h2 = std::hash<uint64_t>{}(pair.B);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

    // Handles 2D physics simulation using broad and narrow phase collision workflow
    class PhysicsSystem : public ISystem {
    public:
        // Uses default construction because runtime caches are value initialized members
        PhysicsSystem() = default;

        // Uses default destruction because owned state is standard containers and smart pointers
        ~PhysicsSystem() override = default;

        // ISystem interface

        // Called when the system is created and currently keeps no world owned state
        void OnCreate(World& world) override { (void)world; }

        // Runs one physics update tick including collision and tilemap synchronization logic
        void OnUpdate(World& world) override;

        // Called before system shutdown to release runtime references and cached state
        void OnDestroy(World& world) override;

        // Returns metadata describing dependencies access patterns and execution ordering
        SystemMetadata GetMetadata() const override;

        // Declares this system as part of physics group for scheduler grouping
        SystemGroup GetSystemGroup() const override { return SystemGroup::Physics; }

        // Restricts this system to play mode so editor mode stays deterministic and non-simulated
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

    private:
        friend class PhysicsPipelineRunner;

        // Caches runtime tilemap state needed for collision and physics synchronization
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

        // Refreshes runtime tilemap cache by reconciling world tilemap components with cached entries
        void RefreshRuntimeTileMaps(World& world);

        // Stores collision pairs from previous frame to drive enter and exit event transitions
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> m_previousCollisions;

        // Stores trigger overlaps from previous frame for trigger enter and exit tracking
        std::unordered_set<PackedEntityPair, PackedEntityPairHash> m_previousTriggerOverlaps;

        // Runtime tilemap cache keyed by entity id for fast physics queries and sync checks
        std::unordered_map<EntityId, RuntimeTileMapEntry> m_runtimeTileMaps;
    };
}

#endif  // PHYSICS2D_H
