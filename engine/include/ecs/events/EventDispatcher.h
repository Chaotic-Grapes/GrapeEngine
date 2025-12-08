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
         * Adds CollisionEvent components to both entities.
         */
        void FireCollisionEvent(
            EntityId entity1Id, // ID of the first entity
            EntityId entity2Id, // ID of the second entity
            const Vector3D& contactPoint, // Contact point in world space
            const Vector3D& contactNormal, // Contact normal (points from entity1 to entity2)
            const Vector3D& relativeVelocity, // Relative velocity at contact
            float impulseMagnitude // Magnitude of the impulse
        );

        /**
         * @brief Fire a trigger enter event.
         * Adds a TriggerEvent component to the trigger entity.
         * 
         * @param triggerId ID of the trigger entity
         * @param otherEntityId ID of the other entity entering the trigger
         */
        void FireTriggerEnterEvent(EntityId triggerId, EntityId otherEntityId);

        /**
         * @brief Fire a trigger stay event (entity still overlapping).
         * Adds a TriggerEvent component to the trigger entity.
         * 
         * @param triggerId ID of the trigger entity
         * @param otherEntityId ID of the other entity staying in the trigger
         */
        void FireTriggerStayEvent(EntityId triggerId, EntityId otherEntityId);

        /**
         * @brief Fire a collision exit event between two entities.
         * Adds CollisionExitEvent components to both entities.
         * 
         * @param entity1Id ID of the first entity
         * @param entity2Id ID of the second entity
         * @param lastContactPoint Last contact point in world space
         */
        void FireCollisionExitEvent(
            EntityId entity1Id,
            EntityId entity2Id,
            const Vector3D& lastContactPoint
        );

        /**
         * @brief Fire a trigger exit event.
         * Adds a TriggerExitEvent component to the trigger entity.
         * 
         * @param triggerId ID of the trigger entity
         * @param otherEntityId ID of the other entity exiting the trigger
         */
        void FireTriggerExitEvent(EntityId triggerId, EntityId otherEntityId);

        /**
         * @brief Clear all event components added this frame.
         * Called at the end of each frame to remove event components.
         */
        void ClearFrameEvents();

    private:
        World* world;

        // Track entities that received event components this frame
        std::vector<EntityId> collisionEventEntities;
        std::vector<EntityId> triggerEventEntities;
        std::vector<EntityId> collisionExitEventEntities;
        std::vector<EntityId> triggerExitEventEntities;
    };
}

#endif
