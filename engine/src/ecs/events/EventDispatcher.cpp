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
#include "helpers/EntityUtils.h"

namespace {
    /**
     * @brief Append an event to the entity's event buffer, creating it if necessary.
     *        If the buffer is full, the event is not added.
     * @tparam TBuffer The type of the event buffer component.
     * @tparam TEvent The type of the event to append.
     * @param world Pointer to the ECS world.
     * @param entity The entity to add the event to.
     * @param entityId The packed ID of the entity.
     * @param trackedEntities Vector to track entities that received event buffers.
     * @param evt The event to append.
     */
    template <typename TBuffer, typename TEvent>
    void AppendEvent(ECS::World* world,
                     const ECS::Entity entity,
                     const PackedEntityId entityId,
                     std::vector<PackedEntityId>& trackedEntities,
                     const TEvent& evt) {
        if (!world || !world->IsAlive(entity))
            return;

        // Try to get existing buffer
        if (auto* buffer = world->TryGet<TBuffer>(entity)) {
            // Append event if there's space
            if (buffer->Count < ECS::Events::kMaxEventBufferSize) {
                // Add event to buffer
                buffer->Events[buffer->Count++] = evt;
            }
            return;
        }

        // No existing buffer; create a new one
        TBuffer newBuffer{};
        newBuffer.Count = 1;
        newBuffer.Events[0] = evt;
        world->Add<TBuffer>(entity, newBuffer);
        trackedEntities.push_back(entityId);
    }
}

namespace ECS::Events {
    EventDispatcher::EventDispatcher(World* world) : world(world) { }

    EventDispatcher::~EventDispatcher() {
    }

    /**
     * @brief Dispatch a collision event to both participating entities, appending it to each
     *        entity's CollisionEventBuffer with appropriate normal/velocity inversion for entity2.
     * @param entity1Id Packed ID of the first entity involved in the collision.
     * @param entity2Id Packed ID of the second entity involved in the collision.
     * @param contactPoint World-space point of contact.
     * @param contactNormal Collision normal pointing from entity2 toward entity1.
     * @param relativeVelocity Relative velocity at the point of contact.
     * @param impulseMagnitude Magnitude of the collision impulse applied.
     */
    void EventDispatcher::FireCollisionEvent(
        PackedEntityId entity1Id,
        PackedEntityId entity2Id,
        const Vector3D& contactPoint,
        const Vector3D& contactNormal,
        const Vector3D& relativeVelocity,
        float impulseMagnitude
    ) {
        if (!world)
            return;

        // Add CollisionEvent to entity1 buffer
        Entity entity1 = ECS::EntityUtils::Unpack(entity1Id);
        if (!world->IsAlive(entity1))
            return;
        CollisionEvent event1;
        event1.OtherEntityId = entity2Id;
        event1.ContactPoint = contactPoint;
        event1.ContactNormal = contactNormal;
        event1.RelativeVelocity = relativeVelocity;
        event1.ImpulseMagnitude = impulseMagnitude;
        AppendEvent<CollisionEventBuffer>(world, entity1, entity1Id, collisionEventEntities, event1);

        // Add CollisionEvent to entity2 buffer with inverted normal
        Entity entity2 = ECS::EntityUtils::Unpack(entity2Id);
        if (!world->IsAlive(entity2))
            return;
        CollisionEvent event2;
        event2.OtherEntityId = entity1Id;
        event2.ContactPoint = contactPoint;
        event2.ContactNormal = -contactNormal;  // Inverted for other entity
        event2.RelativeVelocity = -relativeVelocity;  // Inverted
        event2.ImpulseMagnitude = impulseMagnitude;
        AppendEvent<CollisionEventBuffer>(world, entity2, entity2Id, collisionEventEntities, event2);
    }

    /**
     * @brief Fire a trigger-enter event on the trigger entity, recording the other entity as the entering body.
     * @param triggerId Packed ID of the trigger entity.
     * @param otherEntityId Packed ID of the entity that entered the trigger.
     */
    void EventDispatcher::FireTriggerEnterEvent(PackedEntityId triggerId, PackedEntityId otherEntityId) {
        if (!world)
            return;

        Entity trigger = ECS::EntityUtils::Unpack(triggerId);
        if (!world->IsAlive(trigger))
            return;
        TriggerEvent event;
        event.OtherEntityId = otherEntityId;
        event.IsEnter = true;
        event.IsActive = true;
        AppendEvent<TriggerEventBuffer>(world, trigger, triggerId, triggerEventEntities, event);
    }

    /**
     * @brief Fire a trigger-stay event on the trigger entity, recording the other entity as the overlapping body.
     * @param triggerId Packed ID of the trigger entity.
     * @param otherEntityId Packed ID of the entity staying inside the trigger.
     */
    void EventDispatcher::FireTriggerStayEvent(PackedEntityId triggerId, PackedEntityId otherEntityId) {
        if (!world)
            return;

        Entity trigger = ECS::EntityUtils::Unpack(triggerId);
        if (!world->IsAlive(trigger))
            return;
        TriggerEvent event;
        event.OtherEntityId = otherEntityId;
        event.IsEnter = false;
        event.IsActive = true;
        AppendEvent<TriggerEventBuffer>(world, trigger, triggerId, triggerEventEntities, event);
    }

    /**
     * @brief Dispatch a collision-exit event to both participating entities.
     * @param entity1Id Packed ID of the first entity.
     * @param entity2Id Packed ID of the second entity.
     * @param lastContactPoint The last known contact point before separation.
     */
    void EventDispatcher::FireCollisionExitEvent(
        PackedEntityId entity1Id,
        PackedEntityId entity2Id,
        const Vector3D& lastContactPoint
    ) {
        if (!world)
            return;

        // Add CollisionExitEvent to entity1 buffer
        Entity entity1 = ECS::EntityUtils::Unpack(entity1Id);
        if (!world->IsAlive(entity1))
            return;
        CollisionExitEvent event1;
        event1.OtherEntityId = entity2Id;
        event1.LastContactPoint = lastContactPoint;
        AppendEvent<CollisionExitEventBuffer>(world, entity1, entity1Id, collisionExitEventEntities, event1);

        // Add CollisionExitEvent to entity2 buffer
        Entity entity2 = ECS::EntityUtils::Unpack(entity2Id);
        if (!world->IsAlive(entity2))
            return;
        CollisionExitEvent event2;
        event2.OtherEntityId = entity1Id;
        event2.LastContactPoint = lastContactPoint;
        AppendEvent<CollisionExitEventBuffer>(world, entity2, entity2Id, collisionExitEventEntities, event2);
    }

    /**
     * @brief Fire a trigger-exit event on the trigger entity, recording the other entity as the leaving body.
     * @param triggerId Packed ID of the trigger entity.
     * @param otherEntityId Packed ID of the entity that exited the trigger.
     */
    void EventDispatcher::FireTriggerExitEvent(PackedEntityId triggerId, PackedEntityId otherEntityId) {
        if (!world)
            return;

        Entity trigger = ECS::EntityUtils::Unpack(triggerId);
        if (!world->IsAlive(trigger))
            return;
        TriggerExitEvent event;
        event.OtherEntityId = otherEntityId;
        AppendEvent<TriggerExitEventBuffer>(world, trigger, triggerId, triggerExitEventEntities, event);
    }

    /**
     * @brief Remove all per-frame event buffer components from every entity that received events
     *        this frame and clear the internal tracking lists.
     */
    void EventDispatcher::ClearFrameEvents() {
        if (!world)
            return;

        // Clear all CollisionEvent buffers
        for (PackedEntityId entityId : collisionEventEntities)
        {
            Entity entity = ECS::EntityUtils::Unpack(entityId);
            if (world->IsAlive(entity) && world->Has<CollisionEventBuffer>(entity))
            {
                world->Remove<CollisionEventBuffer>(entity);
            }
        }
        collisionEventEntities.clear();

        // Clear all TriggerEvent buffers
        for (PackedEntityId entityId : triggerEventEntities)
        {
            Entity entity = ECS::EntityUtils::Unpack(entityId);
            if (world->IsAlive(entity) && world->Has<TriggerEventBuffer>(entity))
            {
                world->Remove<TriggerEventBuffer>(entity);
            }
        }
        triggerEventEntities.clear();

        // Clear all CollisionExitEvent buffers
        for (PackedEntityId entityId : collisionExitEventEntities)
        {
            Entity entity = ECS::EntityUtils::Unpack(entityId);
            if (world->IsAlive(entity) && world->Has<CollisionExitEventBuffer>(entity))
            {
                world->Remove<CollisionExitEventBuffer>(entity);
            }
        }
        collisionExitEventEntities.clear();

        // Clear all TriggerExitEvent buffers
        for (PackedEntityId entityId : triggerExitEventEntities)
        {
            Entity entity = ECS::EntityUtils::Unpack(entityId);
            if (world->IsAlive(entity) && world->Has<TriggerExitEventBuffer>(entity))
            {
                world->Remove<TriggerExitEventBuffer>(entity);
            }
        }
        triggerExitEventEntities.clear();
    }

}
