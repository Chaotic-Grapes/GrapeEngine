/* Start Header *****************************************************************/
/*!
\file   ContactManager2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Collision and trigger event transition management.
\references
- https://box2d.org/documentation/
- https://box2d.org/documentation/md_simulation.html#autotoc_md127
- https://docs.unity3d.com/Manual/collider-interactions-oncollision.html
- https://docs.unity3d.com/Manual/collider-interactions-ontrigger.html
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
        auto pairLess = [](const PairKey2D& lhs, const PairKey2D& rhs) {
            if (lhs.A != rhs.A) return lhs.A < rhs.A;
            return lhs.B < rhs.B;
            };

        std::unordered_set<PairKey2D, PairKeyHash> frameCollisions;
        std::unordered_set<PairKey2D, PairKeyHash> frameTriggerOverlaps;
        // Reserve upfront to reduce hash-table reallocation churn each frame.
        frameCollisions.reserve(contacts.size());
        frameTriggerOverlaps.reserve(contacts.size());

        for (const auto& c : contacts) {
            if (c.IsTrigger) {
                // Track trigger pairs directionally (trigger entity stored first).
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

        std::vector<PairKey2D> previousCollisionsSorted(m_previousCollisions.begin(), m_previousCollisions.end());
        std::sort(previousCollisionsSorted.begin(), previousCollisionsSorted.end(), pairLess);
        for (const auto& pair : previousCollisionsSorted) {
            if (frameCollisions.contains(pair)) {
                // Still colliding this frame; no exit event.
                continue;
            }
            const ECS::Entity a = ECS::EntityUtils::Unpack(pair.A);
            const ECS::Entity b = ECS::EntityUtils::Unpack(pair.B);
            if (!world.IsAlive(a) || !world.IsAlive(b)) {
                // Skip exits for entities that were destroyed before event dispatch.
                continue;
            }
            dispatcher.FireCollisionExitEvent(pair.A, pair.B, Vector3D(0.0f, 0.0f, 0.0f));
        }

        std::vector<PairKey2D> frameTriggerSorted(frameTriggerOverlaps.begin(), frameTriggerOverlaps.end());
        std::sort(frameTriggerSorted.begin(), frameTriggerSorted.end(), pairLess);
        for (const auto& pair : frameTriggerSorted) {
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
                // New overlap this frame -> trigger enter.
                dispatcher.FireTriggerEnterEvent(pair.A, pair.B);
            }
        }

        std::vector<PairKey2D> previousTriggerSorted(m_previousTriggerOverlaps.begin(), m_previousTriggerOverlaps.end());
        std::sort(previousTriggerSorted.begin(), previousTriggerSorted.end(), pairLess);
        for (const auto& pair : previousTriggerSorted) {
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

        // Current frame becomes previous frame for next transition detection.
        m_previousCollisions = std::move(frameCollisions);
        m_previousTriggerOverlaps = std::move(frameTriggerOverlaps);
    }
}
