/* Start Header *****************************************************************/
/*!
\file    WorldInterop_Collision.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of Collision event interop functions for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#define BUILDING_WORLD_INTEROP
#include "scripting/WorldInterop.h"
#include "ecs/World.h"
#include "physics/CollisionEvents.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"

namespace {
    // Convert World pointer
    ECS::World* GetWorld(void* worldPtr) {
        return static_cast<ECS::World*>(worldPtr);
    }
}

// ============================================================================
// Collision Events
// ============================================================================

WORLD_INTEROP_API void WorldInterop_Collision_Clear(void* worldPtr) {
    if (!worldPtr) {
        LOG_WARNING("[WorldInterop_Collision] World pointer is null");
        return;
    }

    // Note: CollisionEventQueue is global, but we still validate world pointer
    ECS::CollisionEventQueue::Clear();
}

WORLD_INTEROP_API uint32_t WorldInterop_Collision_GetEventCount(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        LOG_WARNING("[WorldInterop_Collision] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return 0;
    }

    const auto& events = ECS::CollisionEventQueue::GetEvents(entity);
    return static_cast<uint32_t>(events.size());
}

WORLD_INTEROP_API bool WorldInterop_Collision_GetEvent(void* worldPtr, uint64_t entityId, uint32_t index, uint64_t* outOtherEntityId, int* outEventType) {
    if (!worldPtr || !outOtherEntityId || !outEventType) {
        LOG_WARNING("[WorldInterop_Collision] Invalid parameters");
        return false;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return false;
    }

    const auto& events = ECS::CollisionEventQueue::GetEvents(entity);
    if (index >= events.size()) {
        return false;
    }

    const auto& collisionEvent = events[index];
    *outOtherEntityId = ECS::EntityUtils::Pack(collisionEvent.OtherEntity);
    *outEventType = static_cast<int>(collisionEvent.Type);
    
    return true;
}

WORLD_INTEROP_API bool WorldInterop_Collision_GetEventsBulk(void* worldPtr, uint64_t entityId, uint64_t* outOtherEntityIds, int* outEventTypes, uint32_t maxCount, uint32_t* outActualCount) {
    if (!worldPtr || !outOtherEntityIds || !outEventTypes || !outActualCount) {
        LOG_WARNING("[WorldInterop_Collision] Invalid parameters");
        return false;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        *outActualCount = 0;
        return false;
    }

    const auto& events = ECS::CollisionEventQueue::GetEvents(entity);
    uint32_t count = std::min(static_cast<uint32_t>(events.size()), maxCount);
    
    for (uint32_t i = 0; i < count; ++i) {
        outOtherEntityIds[i] = ECS::EntityUtils::Pack(events[i].OtherEntity);
        outEventTypes[i] = static_cast<int>(events[i].Type);
    }
    
    *outActualCount = count;
    return true;
}

WORLD_INTEROP_API bool WorldInterop_Collision_HasCollisionWith(void* worldPtr, uint64_t entityId, uint64_t otherEntityId) {
    if (!worldPtr) {
        return false;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    ECS::Entity otherEntity = ECS::EntityUtils::Unpack(otherEntityId);
    
    if (!world->IsAlive(entity) || !world->IsAlive(otherEntity)) {
        return false;
    }

    const auto& events = ECS::CollisionEventQueue::GetEvents(entity);
    
    for (const auto& evt : events) {
        if (evt.OtherEntity.GetID() == otherEntity.GetID()) {
            return true;
        }
    }
    
    return false;
}

WORLD_INTEROP_API int WorldInterop_Collision_GetCollisionType(void* worldPtr, uint64_t entityId, uint64_t otherEntityId) {
    if (!worldPtr) {
        return -1;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    ECS::Entity otherEntity = ECS::EntityUtils::Unpack(otherEntityId);
    
    if (!world->IsAlive(entity) || !world->IsAlive(otherEntity)) {
        return -1;
    }

    const auto& events = ECS::CollisionEventQueue::GetEvents(entity);
    
    for (const auto& evt : events) {
        if (evt.OtherEntity.GetID() == otherEntity.GetID()) {
            return static_cast<int>(evt.Type);
        }
    }
    
    return -1; // No collision found
}
