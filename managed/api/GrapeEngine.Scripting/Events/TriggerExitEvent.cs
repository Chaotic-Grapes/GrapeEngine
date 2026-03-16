using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Event component fired when a trigger stops overlapping another collider.
/// This component is automatically added by the physics system when trigger overlap ends
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to trigger exit:
/// <code>
/// foreach (var (entity, buffer) in world.Query&lt;TriggerExitEventBuffer&gt;())
/// {
///     for (int i = 0; i &lt; buffer.Count; ++i)
///         Console.WriteLine($"Trigger left by {buffer.GetEvent(i).OtherEntityId}");
/// }
/// </code>
/// </summary>
public struct TriggerExitEvent
{
    /// <summary>
    /// The entity ID that stopped overlapping with this trigger.
    /// </summary>
    public ulong OtherEntityId { get; set; }
}
