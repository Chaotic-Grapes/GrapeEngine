/* Start Header *****************************************************************/
/*!
\file    EventDispatcher.cpp
\author  Dalton Koh (100%)
\par     d.koh.b@digipen.edu
\brief
Declaration of the EventDispatcher class for managing ECS events.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_EVENTS_EVENTDISPATCHER_H
#define ECS_EVENTS_EVENTDISPATCHER_H

#include "EventComponents.h"
#include "ecs/World.h"
#include "math/Vector3D.h"
#include <vector>
#include <cstdint>

namespace ECS::Events {
  
    /**
     * @brief Dispatcher for ECS event components (collisions, triggers).
     * Used by the physics system to fire events by adding components to entities.
     * Event components are automatically removed at the end of each frame.
     */
    class EventDispatcher {
    public:
        explicit EventDispatcher(World* world);
        ~EventDispatcher();

        /**
         * @brief Fire a collision event between two entities.
         * Appends CollisionEvent entries to the per-entity buffers.
         */
        void FireCollisionEvent(
            PackedEntityId entity1Id, // Packed ID of the first entity
            PackedEntityId entity2Id, // Packed ID of the second entity
            const Vector3D& contactPoint, // Contact point in world space
            const Vector3D& contactNormal, // Contact normal (points from entity1 to entity2)
            const Vector3D& relativeVelocity, // Relative velocity at contact
            float impulseMagnitude // Magnitude of the impulse
        );

        /**
         * @brief Fire a trigger enter event.
         * Appends a TriggerEvent entry to the trigger's buffer.
         * 
         * @param triggerId ID of the trigger entity
         * @param otherEntityId ID of the other entity entering the trigger
         */
        void FireTriggerEnterEvent(PackedEntityId triggerId, PackedEntityId otherEntityId);

        /**
         * @brief Fire a trigger stay event (entity still overlapping).
         * Appends a TriggerEvent entry to the trigger's buffer.
         * 
         * @param triggerId ID of the trigger entity
         * @param otherEntityId ID of the other entity staying in the trigger
         */
        void FireTriggerStayEvent(PackedEntityId triggerId, PackedEntityId otherEntityId);

        /**
         * @brief Fire a collision exit event between two entities.
         * Appends CollisionExitEvent entries to the per-entity buffers.
         * 
         * @param entity1Id ID of the first entity
         * @param entity2Id ID of the second entity
         * @param lastContactPoint Last contact point in world space
         */
        void FireCollisionExitEvent(
            PackedEntityId entity1Id,
            PackedEntityId entity2Id,
            const Vector3D& lastContactPoint
        );

        /**
         * @brief Fire a trigger exit event.
         * Appends a TriggerExitEvent entry to the trigger's buffer.
         * 
         * @param triggerId ID of the trigger entity
         * @param otherEntityId ID of the other entity exiting the trigger
         */
        void FireTriggerExitEvent(PackedEntityId triggerId, PackedEntityId otherEntityId);

        /**
         * @brief Clear all event components added this frame.
         * Called at the end of each frame to remove event buffers.
         */
        void ClearFrameEvents();

    private:
        World* world;

        // Track entities that received event components this frame
        std::vector<PackedEntityId> collisionEventEntities;
        std::vector<PackedEntityId> triggerEventEntities;
        std::vector<PackedEntityId> collisionExitEventEntities;
        std::vector<PackedEntityId> triggerExitEventEntities;
    };

    /**
     * @brief Clear all event components from the world.
     * 
     * Intended to be called once per frame after systems have had a chance
     * to observe collision/trigger events.
     */
    inline void ClearFrameEventComponents(World& world) {
        world.Each<ECS::Events::CollisionEventBuffer>([&](Entity e, ECS::Events::CollisionEventBuffer&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::CollisionEventBuffer>(e))
                world.Remove<ECS::Events::CollisionEventBuffer>(e);
        });
        world.Each<ECS::Events::TriggerEventBuffer>([&](Entity e, ECS::Events::TriggerEventBuffer&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::TriggerEventBuffer>(e))
                world.Remove<ECS::Events::TriggerEventBuffer>(e);
        });
        world.Each<ECS::Events::CollisionExitEventBuffer>([&](Entity e, ECS::Events::CollisionExitEventBuffer&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::CollisionExitEventBuffer>(e))
                world.Remove<ECS::Events::CollisionExitEventBuffer>(e);
        });
        world.Each<ECS::Events::TriggerExitEventBuffer>([&](Entity e, ECS::Events::TriggerExitEventBuffer&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::TriggerExitEventBuffer>(e))
                world.Remove<ECS::Events::TriggerExitEventBuffer>(e);
        });
        world.Each<ECS::Events::AnimationEventBuffer2D>([&](Entity e, ECS::Events::AnimationEventBuffer2D&) {
            if (world.IsAlive(e) && world.Has<ECS::Events::AnimationEventBuffer2D>(e))
                world.Remove<ECS::Events::AnimationEventBuffer2D>(e);
        });
    }
}

#endif
