/* Start Header *****************************************************************/
/*!
\file   UIEvents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th November 2025
\brief
High-level UI events service wrapper for scripts. Provides access to the 
UI event system for handling clicks, hover states, and UI interactions.

\code
// In your script's OnUpdate method:
if (UIEvents.WasClicked(EntityId))
{
    Log("Entity was clicked!");
}

if (UIEvents.WasHoverEntered(EntityId))
{
    Log("Mouse entered entity!");
}

// Get all events for this entity
var events = UIEvents.GetEvents(EntityId);
foreach (var evt in events)
{
    Log($"Event: {evt.Type} at {evt.ScreenPosition}");
}
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Type of UI event that occurred
/// </summary>
public enum UIEventType
{
    /// <summary>
    /// Entity was clicked
    /// </summary>
    Click = 0,

    /// <summary>
    /// Mouse entered entity (hover started)
    /// </summary>
    HoverEnter = 1,

    /// <summary>
    /// Mouse exited entity (hover ended)
    /// </summary>
    HoverExit = 2
}

/// <summary>
/// High-level UI events utilities for detecting mouse interactions with entities.
/// Uses the new World-based architecture.
/// </summary>
public static class UIEvents
{
    /// <summary>
    /// Clear all UI events (typically called at start of frame).
    /// </summary>
    public static unsafe void Clear(World world)
    {
        UIEventsAPI.Clear(world.NativePtr);
    }

    /// <summary>
    /// Check if an entity was clicked this frame.
    /// </summary>
    public static unsafe bool WasClicked(World world, Entity entity, int button = 0)
    {
        return UIEventsAPI.WasClicked(world.NativePtr, entity.Id, button);
    }

    /// <summary>
    /// Check if an entity had hover enter this frame.
    /// </summary>
    public static unsafe bool WasHoverEntered(World world, Entity entity)
    {
        return UIEventsAPI.WasHoverEntered(world.NativePtr, entity.Id);
    }

    /// <summary>
    /// Check if an entity had hover exit this frame.
    /// </summary>
    public static unsafe bool WasHoverExited(World world, Entity entity)
    {
        return UIEventsAPI.WasHoverExited(world.NativePtr, entity.Id);
    }

    /// <summary>
    /// Get the number of UI events this frame.
    /// </summary>
    public static unsafe int GetEventCount(World world)
    {
        return UIEventsAPI.GetEventCount(world.NativePtr);
    }

    /// <summary>
    /// Get a UI event by index.
    /// </summary>
    public static unsafe (Entity entity, UIEventType eventType) GetEvent(World world, int index)
    {
        ulong entityId;
        int eventType;
        UIEventsAPI.GetEvent(world.NativePtr, index, &entityId, &eventType);
        return (new Entity(world, entityId), (UIEventType)eventType);
    }

    /// <summary>
    /// Get the entity currently being hovered.
    /// </summary>
    public static unsafe Entity? GetHoveredEntity(World world)
    {
        ulong entityId = UIEventsAPI.GetHoveredEntity(world.NativePtr);
        return entityId != 0 ? new Entity(world, entityId) : null;
    }

    /// <summary>
    /// Check if the mouse is over any UI element.
    /// </summary>
    public static unsafe bool IsMouseOverUI(World world)
    {
        return UIEventsAPI.IsMouseOverUI(world.NativePtr);
    }

    /// <summary>
    /// Check if a specific entity is currently hovered.
    /// </summary>
    public static bool IsHovered(World world, Entity entity)
    {
        var hovered = GetHoveredEntity(world);
        return hovered != null && hovered.Id == entity.Id;
    }
}
