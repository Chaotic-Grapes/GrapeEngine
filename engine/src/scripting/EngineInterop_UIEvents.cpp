/* Start Header *****************************************************************/
/*!
\file    EngineInterop_UIEvents.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    26th November 2025
\brief
C API exports for managed C# scripting systems for UI event system operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/UIEventSystem.h"
#include "services/UIEvents.h"
#include "ecs/World.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"

// Export macro for C API
#ifdef _WIN32
    #ifdef BUILDING_ENGINE_INTEROP
        #define ENGINE_INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define ENGINE_INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define ENGINE_INTEROP_API extern "C"
#endif

// External world access (defined in EngineInterop_Component.cpp)
extern ECS::World* g_scriptWorld;

// ============================================================================
// UI Events API - Event Queue Management
// ============================================================================

/// <summary>
/// Clear all UI events from the queue (typically called at start of frame)
/// </summary>
ENGINE_INTEROP_API void EngineInterop_UIEvents_Clear() {
    ECS::UIEventQueue::Clear();
}

/// <summary>
/// Check if an entity was clicked this frame
/// </summary>
/// <param name="entityId">Packed entity ID</param>
/// <param name="button">Mouse button (MOUSE_LEFT=0, MOUSE_RIGHT=1, MOUSE_MIDDLE=2)</param>
/// <returns>True if the entity was clicked with the specified button</returns>
ENGINE_INTEROP_API bool EngineInterop_UIEvents_WasClicked(uint64_t entityId, int button) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return false;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        return false;
    }

    return ECS::UIEventQueue::WasClicked(entity, button);
}

/// <summary>
/// Check if the mouse entered an entity this frame (hover started)
/// </summary>
/// <param name="entityId">Packed entity ID</param>
/// <returns>True if hover started this frame</returns>
ENGINE_INTEROP_API bool EngineInterop_UIEvents_WasHoverEntered(uint64_t entityId) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return false;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        return false;
    }

    return ECS::UIEventQueue::WasHoverEntered(entity);
}

/// <summary>
/// Check if the mouse exited an entity this frame (hover ended)
/// </summary>
/// <param name="entityId">Packed entity ID</param>
/// <returns>True if hover ended this frame</returns>
ENGINE_INTEROP_API bool EngineInterop_UIEvents_WasHoverExited(uint64_t entityId) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return false;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        return false;
    }

    return ECS::UIEventQueue::WasHoverExited(entity);
}

/// <summary>
/// Get the number of UI events for a specific entity this frame
/// </summary>
/// <param name="entityId">Packed entity ID</param>
/// <returns>Number of events for this entity</returns>
ENGINE_INTEROP_API int EngineInterop_UIEvents_GetEventCount(uint64_t entityId) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return 0;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        return 0;
    }

    const auto& events = ECS::UIEventQueue::GetEvents(entity);
    return static_cast<int>(events.size());
}

/// <summary>
/// Get details of a specific event by index
/// </summary>
/// <param name="entityId">Packed entity ID</param>
/// <param name="eventIndex">Index of the event to retrieve</param>
/// <param name="outType">Output: Event type (0=Click, 1=HoverEnter, 2=HoverExit)</param>
/// <param name="outButton">Output: Mouse button (or -1 for hover events)</param>
/// <param name="outScreenX">Output: Screen X position</param>
/// <param name="outScreenY">Output: Screen Y position</param>
/// <returns>True if event was retrieved successfully</returns>
ENGINE_INTEROP_API bool EngineInterop_UIEvents_GetEvent(uint64_t entityId, int eventIndex, 
    int* outType, int* outButton, float* outScreenX, float* outScreenY) {
    
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return false;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        return false;
    }

    const auto& events = ECS::UIEventQueue::GetEvents(entity);
    if (eventIndex < 0 || eventIndex >= static_cast<int>(events.size())) {
        return false;
    }

    const auto& event = events[eventIndex];
    
    if (outType) *outType = static_cast<int>(event.Type);
    if (outButton) *outButton = event.Button;
    if (outScreenX) *outScreenX = event.ScreenPosition.X;
    if (outScreenY) *outScreenY = event.ScreenPosition.Y;

    return true;
}

// ============================================================================
// UI Events API - Hover State Query
// ============================================================================

/// <summary>
/// Get the currently hovered entity ID (0 if none)
/// </summary>
/// <returns>Packed entity ID of the hovered entity, or 0 if none</returns>
ENGINE_INTEROP_API uint64_t EngineInterop_UIEvents_GetHoveredEntity() {
    ECS::Entity hoveredEntity = ECS::UIEventSystem::GetHoveredEntity();
    if (hoveredEntity.IsNull()) {
        return 0;
    }
    return ECS::EntityUtils::Pack(hoveredEntity);
}

/// <summary>
/// Check if the mouse is currently over any UI element
/// </summary>
/// <returns>True if mouse is over a UI element</returns>
ENGINE_INTEROP_API bool EngineInterop_UIEvents_IsMouseOverUI() {
    return ECS::UIEventSystem::IsMouseOverUI();
}
