using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Event component fired when a trigger/sensor overlaps another collider.
/// This component is automatically added by the physics system during trigger overlap
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to trigger events:
/// <code>
/// foreach (var (entity, buffer) in world.Query&lt;TriggerEventBuffer&gt;())
/// {
///     for (int i = 0; i &lt; buffer.Count; ++i)
///     {
///         if (buffer.GetEvent(i).IsEnter)
///             Console.WriteLine("Trigger entered!");
///     }
/// }
/// </code>
/// </summary>
public struct TriggerEvent
{
    /// <summary>
    /// The entity ID that triggered the overlap.
    /// </summary>
    public ulong OtherEntityId { get; set; }

    /// <summary>
    /// True if this is the first frame of overlap (enter event).
    /// False if the entities are still overlapping from previous frames.
    /// </summary>
    public bool IsEnter { get; set; }

    /// <summary>
    /// True if the overlap is currently active.
    /// </summary>
    public bool IsActive { get; set; }
}
