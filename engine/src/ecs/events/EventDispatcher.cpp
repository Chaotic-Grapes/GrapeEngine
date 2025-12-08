/* Start Header *****************************************************************/
/*!
\file    EventDispatcher.cpp
\author  Dalton Koh (100%)
\par     d.koh.b@digipen.edu
\brief
Implementation of the EventDispatcher class for managing ECS events.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/events/EventDispatcher.h"

namespace ECS::Events {
    EventDispatcher::EventDispatcher(World* world) : world(world) { }

    EventDispatcher::~EventDispatcher() {
        ClearFrameEvents();
    }

    void EventDispatcher::FireCollisionEvent(
        EntityId entity1Id,
        EntityId entity2Id,
        const Vector3D& contactPoint,
        const Vector3D& contactNormal,
        const Vector3D& relativeVelocity,
        float impulseMagnitude
    ) {
        if (!world)
            return;

        // Add CollisionEvent to entity1
        Entity entity1 = world->Resolve(entity1Id);
        if (world->IsAlive(entity1))
        {
            CollisionEvent event1;
            event1.OtherEntityId = entity2Id;
            event1.ContactPoint = contactPoint;
            event1.ContactNormal = contactNormal;
            event1.RelativeVelocity = relativeVelocity;
            event1.ImpulseMagnitude = impulseMagnitude;
            world->Add<CollisionEvent>(entity1, event1);
            collisionEventEntities.push_back(entity1Id);
        }

        // Add CollisionEvent to entity2 with inverted normal
        Entity entity2 = world->Resolve(entity2Id);
        if (world->IsAlive(entity2))
        {
            CollisionEvent event2;
            event2.OtherEntityId = entity1Id;
            event2.ContactPoint = contactPoint;
            event2.ContactNormal = -contactNormal;  // Inverted for other entity
            event2.RelativeVelocity = -relativeVelocity;  // Inverted
            event2.ImpulseMagnitude = impulseMagnitude;
            world->Add<CollisionEvent>(entity2, event2);
            collisionEventEntities.push_back(entity2Id);
        }
    }

    void EventDispatcher::FireTriggerEnterEvent(EntityId triggerId, EntityId otherEntityId) {
        if (!world)
            return;

        Entity trigger = world->Resolve(triggerId);
        if (world->IsAlive(trigger))
        {
            TriggerEvent event;
            event.OtherEntityId = otherEntityId;
            event.IsEnter = true;
            event.IsActive = true;
            world->Add<TriggerEvent>(trigger, event);
            triggerEventEntities.push_back(triggerId);
        }
    }

    void EventDispatcher::FireTriggerStayEvent(EntityId triggerId, EntityId otherEntityId) {
        if (!world)
            return;

        Entity trigger = world->Resolve(triggerId);
        if (world->IsAlive(trigger))
        {
            if (auto* triggerEvent = world->TryGet<TriggerEvent>(trigger))
            {
                triggerEvent->IsEnter = false;
                triggerEvent->IsActive = true;
            }
        }
    }

    void EventDispatcher::FireCollisionExitEvent(
        EntityId entity1Id,
        EntityId entity2Id,
        const Vector3D& lastContactPoint
    ) {
        if (!world)
            return;

        // Add CollisionExitEvent to entity1
        Entity entity1 = world->Resolve(entity1Id);
        if (world->IsAlive(entity1))
        {
            CollisionExitEvent event1;
            event1.OtherEntityId = entity2Id;
            event1.LastContactPoint = lastContactPoint;
            world->Add<CollisionExitEvent>(entity1, event1);
            collisionExitEventEntities.push_back(entity1Id);
        }

        // Add CollisionExitEvent to entity2
        Entity entity2 = world->Resolve(entity2Id);
        if (world->IsAlive(entity2))
        {
            CollisionExitEvent event2;
            event2.OtherEntityId = entity1Id;
            event2.LastContactPoint = lastContactPoint;
            world->Add<CollisionExitEvent>(entity2, event2);
            collisionExitEventEntities.push_back(entity2Id);
        }
    }

    void EventDispatcher::FireTriggerExitEvent(EntityId triggerId, EntityId otherEntityId) {
        if (!world)
            return;

        Entity trigger = world->Resolve(triggerId);
        if (world->IsAlive(trigger))
        {
            TriggerExitEvent event;
            event.OtherEntityId = otherEntityId;
            world->Add<TriggerExitEvent>(trigger, event);
            triggerExitEventEntities.push_back(triggerId);
        }
    }

    void EventDispatcher::ClearFrameEvents() {
        if (!world)
            return;

        // Clear all CollisionEvent components
        for (EntityId entityId : collisionEventEntities)
        {
            Entity entity = world->Resolve(entityId);
            if (world->IsAlive(entity) && world->Has<CollisionEvent>(entity))
            {
                world->Remove<CollisionEvent>(entity);
            }
        }
        collisionEventEntities.clear();

        // Clear all TriggerEvent components
        for (EntityId entityId : triggerEventEntities)
        {
            Entity entity = world->Resolve(entityId);
            if (world->IsAlive(entity) && world->Has<TriggerEvent>(entity))
            {
                world->Remove<TriggerEvent>(entity);
            }
        }
        triggerEventEntities.clear();

        // Clear all CollisionExitEvent components
        for (EntityId entityId : collisionExitEventEntities)
        {
            Entity entity = world->Resolve(entityId);
            if (world->IsAlive(entity) && world->Has<CollisionExitEvent>(entity))
            {
                world->Remove<CollisionExitEvent>(entity);
            }
        }
        collisionExitEventEntities.clear();

        // Clear all TriggerExitEvent components
        for (EntityId entityId : triggerExitEventEntities)
        {
            Entity entity = world->Resolve(entityId);
            if (world->IsAlive(entity) && world->Has<TriggerExitEvent>(entity))
            {
                world->Remove<TriggerExitEvent>(entity);
            }
        }
        triggerExitEventEntities.clear();
    }

}
