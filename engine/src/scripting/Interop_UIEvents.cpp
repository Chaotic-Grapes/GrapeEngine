/* Start Header *****************************************************************/
/*!
\file   Interop_UIEvents.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
C API exports for UI event queries in the ECS World.
This provides UI interaction detection for managed C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"

// ============================================================================
// UI Events API
// ============================================================================

/// <summary>
/// Clear all UI events (called at start of frame)
/// </summary>
INTEROP_API void WorldInterop_UIEvents_Clear(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return;
    }
    
    // TODO: Implement UI event clearing
    // This would clear the UI event queue for the frame
}

/// <summary>
/// Check if an entity was clicked this frame
/// </summary>
INTEROP_API bool WorldInterop_UIEvents_WasClicked(void* worldPtr, uint64_t entityId, int button) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return false;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return false;
    }

    // TODO: Implement UI click detection
    // This would check if the entity was clicked with the given mouse button
    return false;
}

/// <summary>
/// Check if an entity had hover enter this frame
/// </summary>
INTEROP_API bool WorldInterop_UIEvents_WasHoverEntered(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return false;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return false;
    }

    // TODO: Implement hover enter detection
    return false;
}

/// <summary>
/// Check if an entity had hover exit this frame
/// </summary>
INTEROP_API bool WorldInterop_UIEvents_WasHoverExited(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return false;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return false;
    }

    // TODO: Implement hover exit detection
    return false;
}

/// <summary>
/// Get the number of UI events this frame
/// </summary>
INTEROP_API int WorldInterop_UIEvents_GetEventCount(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return 0;
    }

    // TODO: Implement event count query
    return 0;
}

/// <summary>
/// Get a UI event by index
/// </summary>
INTEROP_API void WorldInterop_UIEvents_GetEvent(void* worldPtr, int index, uint64_t* outEntityId, int* outEventType) {
    if (!worldPtr || !outEntityId || !outEventType) {
        LOG_ERROR("[WorldInterop] Invalid parameters");
        return;
    }

    // TODO: Implement event retrieval
    *outEntityId = 0;
    *outEventType = 0;
}

/// <summary>
/// Get the entity currently being hovered
/// </summary>
INTEROP_API uint64_t WorldInterop_UIEvents_GetHoveredEntity(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return 0;
    }

    // TODO: Implement hovered entity query
    return 0;
}

/// <summary>
/// Check if the mouse is over any UI element
/// </summary>
INTEROP_API bool WorldInterop_UIEvents_IsMouseOverUI(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return false;
    }

    // TODO: Implement mouse-over-UI detection
    return false;
}
