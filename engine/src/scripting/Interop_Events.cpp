/* Start Header *****************************************************************/
/*!
\file    Interop_Events.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of Event interop functions for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "scripting/ScriptManager.h"
#include "ecs/events/EventDispatcher.h"
#include "ecs/World.h"
#include "math/Vector3D.h"

using namespace ECS;
using namespace ECS::Events;

// ============================================================================
// Collision Events
// ============================================================================

/**
 * @brief Fire a collision event between two entities.
 * Adds CollisionEvent components to both entities.
 * @param worldPtr Pointer to the World instance
 * @param entity1Id Packed ID of the first entity
 * @param entity2Id Packed ID of the second entity
 * @param contactPointX X component of the contact point
 * @param contactPointY Y component of the contact point
 * @param contactPointZ Z component of the contact point
 * @param contactNormalX X component of the contact normal
 * @param contactNormalY Y component of the contact normal
 * @param contactNormalZ Z component of the contact normal
 * @param relativeVelocityX X component of the relative velocity
 * @param relativeVelocityY Y component of the relative velocity
 * @param relativeVelocityZ Z component of the relative velocity
 * @param impulseMagnitude Magnitude of the impulse
 */
INTEROP_API void WorldInterop_FireCollisionEvent(
    void* worldPtr,
    PackedEntityId entity1Id,
    PackedEntityId entity2Id,
    float contactPointX,
    float contactPointY,
    float contactPointZ,
    float contactNormalX,
    float contactNormalY,
    float contactNormalZ,
    float relativeVelocityX,
    float relativeVelocityY,
    float relativeVelocityZ,
    float impulseMagnitude
) {
    if (!worldPtr) return;
    
    World* world = static_cast<World*>(worldPtr);
    EventDispatcher dispatcher(world);
    
    Vector3D contactPoint(contactPointX, contactPointY, contactPointZ);
    Vector3D contactNormal(contactNormalX, contactNormalY, contactNormalZ);
    Vector3D relativeVelocity(relativeVelocityX, relativeVelocityY, relativeVelocityZ);
    
    dispatcher.FireCollisionEvent(
        entity1Id,
        entity2Id,
        contactPoint,
        contactNormal,
        relativeVelocity,
        impulseMagnitude
    );
}

/**
 * @brief Fire a trigger enter event.
 * Adds a TriggerEvent component to the trigger entity.
 * @param worldPtr Pointer to the World instance
 * @param triggerId Packed ID of the trigger entity
 * @param otherEntityId Packed ID of the other entity entering the trigger
 */
INTEROP_API void WorldInterop_FireTriggerEnterEvent(
    void* worldPtr,
    PackedEntityId triggerId,
    PackedEntityId otherEntityId
) {
    if (!worldPtr) return;
    
    World* world = static_cast<World*>(worldPtr);
    EventDispatcher dispatcher(world);
    dispatcher.FireTriggerEnterEvent(triggerId, otherEntityId);
}

/**
 * @brief Fire a trigger stay event (entity still overlapping).
 * Adds a TriggerEvent component to the trigger entity.
 * @param worldPtr Pointer to the World instance
 * @param triggerId Packed ID of the trigger entity
 * @param otherEntityId Packed ID of the other entity staying in the trigger
 */
INTEROP_API void WorldInterop_FireTriggerStayEvent(
    void* worldPtr,
    PackedEntityId triggerId,
    PackedEntityId otherEntityId
) {
    if (!worldPtr) return;
    
    World* world = static_cast<World*>(worldPtr);
    EventDispatcher dispatcher(world);
    dispatcher.FireTriggerStayEvent(triggerId, otherEntityId);
}

/**
 * @brief Fire a collision exit event.
 * Adds CollisionExitEvent components to both entities.
 * @param worldPtr Pointer to the World instance
 * @param entity1Id Packed ID of the first entity
 * @param entity2Id Packed ID of the second entity
 * @param lastContactPointX X component of the last contact point
 * @param lastContactPointY Y component of the last contact point
 * @param lastContactPointZ Z component of the last contact point
 */
INTEROP_API void WorldInterop_FireCollisionExitEvent(
    void* worldPtr,
    PackedEntityId entity1Id,
    PackedEntityId entity2Id,
    float lastContactPointX,
    float lastContactPointY,
    float lastContactPointZ
) {
    if (!worldPtr) return;
    
    World* world = static_cast<World*>(worldPtr);
    EventDispatcher dispatcher(world);
    
    Vector3D lastContactPoint(lastContactPointX, lastContactPointY, lastContactPointZ);
    dispatcher.FireCollisionExitEvent(entity1Id, entity2Id, lastContactPoint);
}

/**
 * @brief Fire a trigger exit event.
 * Adds a TriggerExitEvent component to the trigger entity.
 * @param worldPtr Pointer to the World instance
 * @param triggerId Packed ID of the trigger entity
 * @param otherEntityId Packed ID of the other entity exiting the trigger
 */
INTEROP_API void WorldInterop_FireTriggerExitEvent(
    void* worldPtr,
    PackedEntityId triggerId,
    PackedEntityId otherEntityId
) {
    if (!worldPtr) return;
    
    World* world = static_cast<World*>(worldPtr);
    EventDispatcher dispatcher(world);
    dispatcher.FireTriggerExitEvent(triggerId, otherEntityId);
}

/**
 * @brief Clear all event components added during the frame.
 * This is called at the end of each frame to prevent event components
 * from persisting into the next frame.
 * @param worldPtr Pointer to the World instance
 */
INTEROP_API void WorldInterop_ClearFrameEvents(void* worldPtr) {
    if (!worldPtr) return;
    
    World* world = static_cast<World*>(worldPtr);
    EventDispatcher dispatcher(world);
    dispatcher.ClearFrameEvents();
}
