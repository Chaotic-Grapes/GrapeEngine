/* Start Header *****************************************************************/
/*!
\file   ContactManager2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Contact persistence and deterministic event stream generation.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_CONTACTMANAGER2D_H
#define ENGINE_PHYSICS2D_CONTACTMANAGER2D_H

#include "physics2d/PhysicsData2D.h"
#include "ecs/events/EventDispatcher.h"
#include <unordered_set>
#include <vector>

namespace Engine::Physics2D {
    /**
     * @brief Pair key used for collision/trigger state sets.
     */
    struct PairKey2D {
        PackedEntityId A = 0;
        PackedEntityId B = 0;
        bool operator==(const PairKey2D& other) const { return A == other.A && B == other.B; }
    };

    /**
     * @brief Hash function object for PairKey2D.
     */
    struct PairKeyHash {
        /**
         * @brief Hash function for packed-entity pair keys.
         */
        size_t operator()(const PairKey2D& pair) const noexcept {
            const size_t h1 = std::hash<uint64_t>{}(pair.A);
            const size_t h2 = std::hash<uint64_t>{}(pair.B);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

    class ContactManager2D {
    public:
        /**
         * @brief Emit collision/trigger enter-stay-exit events from this frame's contacts.
         * @param dispatcher Event sink.
         * @param world ECS world used for liveness checks.
         * @param contacts Current frame contact constraints.
         */
        void EmitEvents(
            ECS::Events::EventDispatcher& dispatcher,
            ECS::World& world,
            const std::vector<ContactConstraint2D>& contacts);

    private:
        /**
         * @brief Build canonical unordered key for solid collision pairs.
         */
        static PairKey2D MakeCollisionPair(PackedEntityId a, PackedEntityId b);

        /**
         * @brief Build ordered key for trigger-to-other relation tracking.
         */
        static PairKey2D MakeTriggerPair(PackedEntityId triggerId, PackedEntityId otherId);

        std::unordered_set<PairKey2D, PairKeyHash> m_previousCollisions;
        std::unordered_set<PairKey2D, PairKeyHash> m_previousTriggerOverlaps;
    };
}

#endif
