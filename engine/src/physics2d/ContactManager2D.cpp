/* Start Header *****************************************************************/
/*!
\file   ContactManager2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Collision and trigger event transition management.
*/
/* End Header *******************************************************************/

#include "physics2d/ContactManager2D.h"
#include "helpers/EntityUtils.h"
#include <algorithm>

namespace Engine::Physics2D {
    /**
     * @brief Canonicalize unordered collision pair IDs.
     */
    PairKey2D ContactManager2D::MakeCollisionPair(PackedEntityId a, PackedEntityId b) {
        if (a > b) std::swap(a, b);
        return PairKey2D{ a, b };
    }

    /**
     * @brief Build ordered trigger pair (trigger first).
     */
    PairKey2D ContactManager2D::MakeTriggerPair(PackedEntityId triggerId, PackedEntityId otherId) {
        return PairKey2D{ triggerId, otherId };
    }

    /**
     * @brief Emit collision/trigger enter-stay-exit events for this frame.
     */
    void ContactManager2D::EmitEvents(
        ECS::Events::EventDispatcher& dispatcher,
        ECS::World& world,
        const std::vector<ContactConstraint2D>& contacts)
    {
        std::unordered_set<PairKey2D, PairKeyHash> frameCollisions;
        std::unordered_set<PairKey2D, PairKeyHash> frameTriggerOverlaps;
        frameCollisions.reserve(contacts.size());
        frameTriggerOverlaps.reserve(contacts.size());

        for (const auto& c : contacts) {
            if (c.IsTrigger) {
                if (c.TriggerA) {
                    frameTriggerOverlaps.insert(MakeTriggerPair(c.PackedA, c.PackedB));
                }
                if (c.TriggerB) {
                    frameTriggerOverlaps.insert(MakeTriggerPair(c.PackedB, c.PackedA));
                }
                continue;
            }

            const PairKey2D pair = MakeCollisionPair(c.PackedA, c.PackedB);
            const bool firstSeen = frameCollisions.insert(pair).second;
            const bool wasPreviouslyColliding = m_previousCollisions.contains(pair);
            if (firstSeen && !wasPreviouslyColliding) {
                // Collision enter fires once per pair when transitioning from non-overlap.
                dispatcher.FireCollisionEvent(
                    c.PackedA,
                    c.PackedB,
                    Vector3D(c.Manifold.points[0].X, c.Manifold.points[0].Y, 0.0f),
                    Vector3D(c.Manifold.normal.X, c.Manifold.normal.Y, 0.0f),
                    Vector3D(0.0f, 0.0f, 0.0f),
                    0.0f);
            }
        }

        for (const auto& pair : m_previousCollisions) {
            if (frameCollisions.contains(pair)) {
                continue;
            }
            const ECS::Entity a = ECS::EntityUtils::Unpack(pair.A);
            const ECS::Entity b = ECS::EntityUtils::Unpack(pair.B);
            if (!world.IsAlive(a) || !world.IsAlive(b)) {
                continue;
            }
            dispatcher.FireCollisionExitEvent(pair.A, pair.B, Vector3D(0.0f, 0.0f, 0.0f));
        }

        for (const auto& pair : frameTriggerOverlaps) {
            const ECS::Entity trigger = ECS::EntityUtils::Unpack(pair.A);
            const ECS::Entity other = ECS::EntityUtils::Unpack(pair.B);
            if (!world.IsAlive(trigger) || !world.IsAlive(other)) {
                continue;
            }
            if (m_previousTriggerOverlaps.contains(pair)) {
                // Trigger stay is emitted while overlap persists across frames.
                dispatcher.FireTriggerStayEvent(pair.A, pair.B);
            }
            else {
                dispatcher.FireTriggerEnterEvent(pair.A, pair.B);
            }
        }

        for (const auto& pair : m_previousTriggerOverlaps) {
            if (frameTriggerOverlaps.contains(pair)) {
                continue;
            }
            const ECS::Entity trigger = ECS::EntityUtils::Unpack(pair.A);
            const ECS::Entity other = ECS::EntityUtils::Unpack(pair.B);
            if (!world.IsAlive(trigger) || !world.IsAlive(other)) {
                continue;
            }
            dispatcher.FireTriggerExitEvent(pair.A, pair.B);
        }

        m_previousCollisions = std::move(frameCollisions);
        m_previousTriggerOverlaps = std::move(frameTriggerOverlaps);
    }
}
