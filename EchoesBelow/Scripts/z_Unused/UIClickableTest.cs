using GrapeEngine.Scripting;
using GrapeEngine.Events;
using GrapeEngine.Numerics;

namespace Scripts;

/// <summary>
/// Example script demonstrating how to use `UIEvents` in gameplay scripts.
/// Shows checks for clicks, hover enter/exit, and iterating `GetEvents`.
/// Attach to an entity that has a `UIClickable` and `SpriteRenderer2D`.
/// </summary>
public class UIClickableTest : ScriptBehaviour
{
    private Color _startCol;

    public override void OnStart()
    {
        // Cache the starting color so we can restore it
        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();
        _startCol = sr.Color;

        Log($"UIClickableTest started on entity {EntityId}. Start color: R/{_startCol.R} G/{_startCol.G} B/{_startCol.B} A/{_startCol.A}");
    }

    public override void OnUpdate()
    {
        // Use the entity's ID to query UI events for this entity
        ulong id = EntityId;

        ref SpriteRenderer2D sr = ref GetComponent<SpriteRenderer2D>();

        // 1) Quick checks (no allocations)
        if (UIEvents.WasClicked(id, GrapeEngine.Scripting.MouseButton.Left))
        {
            Log("Entity was left-clicked this frame.");
            // Example reaction: flash red
            sr.Color = new Color(1f, 0f, 0f, 1f);
        }
        else if (UIEvents.WasClicked(id, GrapeEngine.Scripting.MouseButton.Left)) //shld be right
        {
            Log("Entity was right-clicked this frame.");
            sr.Color = new Color(0f, 0f, 1f, 1f);
        }
        else if (UIEvents.WasHoverEntered(id))
        {
            Log("Hover entered this frame.");
            sr.Color = new Color(0f, 1f, 0f, 1f);
        }
        else if (UIEvents.WasHoverExited(id))
        {
            Log("Hover exited this frame.");
            sr.Color = _startCol;
        }
        else
        {
            // If nothing happened to this entity this frame, keep or restore color
            // (You may want to restore after a short timer instead of immediately)
            sr.Color = _startCol;
        }

        // 2) Iterate all events for this entity (allocates an array)
        // Useful if you want to inspect multiple events (e.g., click + hover)
        var events = UIEvents.GetEvents(id);
        if (events.Length > 0)
        {
            foreach (var evt in events)
            {
                switch (evt.Type)
                {
                    case UIEventType.Click:
                        Log($"Event: Click (button {evt.Button}) at {evt.ScreenPosition}");
                        break;
                    case UIEventType.HoverEnter:
                        Log($"Event: HoverEnter at {evt.ScreenPosition}");
                        break;
                    case UIEventType.HoverExit:
                        Log($"Event: HoverExit at {evt.ScreenPosition}");
                        break;
                }
            }
        }

        // 3) Optional: check if the mouse is currently over any UI (useful to block gameplay input)
        if (UIEvents.IsMouseOverUI())
        {
            // Example: prevent other input handling while interacting with UI
            // Log("Mouse is over UI; game input paused.");
        }
    }
}

