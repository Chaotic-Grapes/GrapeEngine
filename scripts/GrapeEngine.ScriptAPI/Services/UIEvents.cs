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

using GrapeEngine.ScriptAPI.Unsafe;
using GrapeEngine.Numerics;

namespace GrapeEngine.Events;

/// <summary>
/// Type of UI event that occurred
/// </summary>
public enum UIEventType
{
    /// <summary>Entity was clicked</summary>
    Click = 0,
    /// <summary>Mouse entered entity (hover started)</summary>
    HoverEnter = 1,
    /// <summary>Mouse exited entity (hover ended)</summary>
    HoverExit = 2
}

/// <summary>
/// Represents a single UI event
/// </summary>
public struct UIEvent
{
    /// <summary>
    /// Type of event that occurred
    /// </summary>
    public UIEventType Type;
    
    /// <summary>
    /// Mouse button involved (or -1 for hover events)
    /// </summary>
    public int Button;
    
    /// <summary>
    /// Screen position where the event occurred
    /// </summary>
    public Vector2 ScreenPosition;

    /// <summary>
    /// Check if this is a click event with a specific button
    /// </summary>
    public bool IsClick(MouseButton button = MouseButton.Left)
        => Type == UIEventType.Click && Button == (int)button;

    /// <summary>
    /// Check if this is a hover enter event
    /// </summary>
    public bool IsHoverEnter => Type == UIEventType.HoverEnter;

    /// <summary>
    /// Check if this is a hover exit event
    /// </summary>
    public bool IsHoverExit => Type == UIEventType.HoverExit;
}

/// <summary>
/// Mouse button constants for UI events
/// </summary>
public enum MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2
}

/// <summary>
/// High-level UI events service for script access.
/// Provides methods to query UI interactions like clicks and hover states.
/// </summary>
public static class UIEvents
{
    /// <summary>
    /// Clear all UI events from the queue (typically called at start of frame by engine)
    /// </summary>
    internal static void Clear() => UIEventsAPI.Clear();

    /// <summary>
    /// Check if an entity was clicked this frame
    /// </summary>
    /// <param name="entityId">Entity ID to check</param>
    /// <param name="button">Mouse button to check (default: Left)</param>
    /// <returns>True if the entity was clicked with the specified button</returns>
    public static bool WasClicked(ulong entityId, MouseButton button = MouseButton.Left)
        => UIEventsAPI.WasClicked(entityId, (int)button);

    /// <summary>
    /// Check if the mouse entered an entity this frame (hover started)
    /// </summary>
    /// <param name="entityId">Entity ID to check</param>
    /// <returns>True if hover started this frame</returns>
    public static bool WasHoverEntered(ulong entityId)
        => UIEventsAPI.WasHoverEntered(entityId);

    /// <summary>
    /// Check if the mouse exited an entity this frame (hover ended)
    /// </summary>
    /// <param name="entityId">Entity ID to check</param>
    /// <returns>True if hover ended this frame</returns>
    public static bool WasHoverExited(ulong entityId)
        => UIEventsAPI.WasHoverExited(entityId);

    /// <summary>
    /// Get all UI events for a specific entity this frame
    /// </summary>
    /// <param name="entityId">Entity ID to get events for</param>
    /// <returns>Array of UI events for this entity</returns>
    public static UIEvent[] GetEvents(ulong entityId)
    {
        int count = UIEventsAPI.GetEventCount(entityId);
        if (count == 0)
            return Array.Empty<UIEvent>();

        var events = new UIEvent[count];
        
        unsafe
        {
            for (int i = 0; i < count; i++)
            {
                int type, button;
                float screenX, screenY;
                
                if (UIEventsAPI.GetEvent(entityId, i, &type, &button, &screenX, &screenY))
                {
                    events[i] = new UIEvent
                    {
                        Type = (UIEventType)type,
                        Button = button,
                        ScreenPosition = new Vector2(screenX, screenY)
                    };
                }
            }
        }

        return events;
    }

    /// <summary>
    /// Get the entity ID that is currently hovered (0 if none)
    /// </summary>
    /// <returns>Entity ID of the hovered entity, or 0 if none</returns>
    public static ulong GetHoveredEntity()
        => UIEventsAPI.GetHoveredEntity();

    /// <summary>
    /// Check if the mouse is currently over any UI element
    /// </summary>
    /// <returns>True if mouse is over a UI element</returns>
    public static bool IsMouseOverUI()
        => UIEventsAPI.IsMouseOverUI();

    /// <summary>
    /// Check if a specific entity is currently hovered
    /// </summary>
    /// <param name="entityId">Entity ID to check</param>
    /// <returns>True if this entity is currently hovered</returns>
    public static bool IsHovered(ulong entityId)
        => GetHoveredEntity() == entityId;
}
