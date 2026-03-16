using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Event component fired when a collision ends (two rigid bodies separate).
/// This component is automatically added by the physics system when collision ends
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to collision exit:
/// <code>
/// foreach (var (entity, buffer) in world.Query&lt;CollisionExitEventBuffer&gt;())
/// {
///     for (int i = 0; i &lt; buffer.Count; ++i)
///         Console.WriteLine($"Stopped colliding with {buffer.GetEvent(i).OtherEntityId}");
/// }
/// </code>
/// </summary>
public struct CollisionExitEvent
{
    /// <summary>
    /// The entity ID that stopped colliding with this entity.
    /// </summary>
    public ulong OtherEntityId { get; set; }

    /// <summary>
    /// The last contact point before the collision ended.
    /// </summary>
    public Vector3 LastContactPoint { get; set; }
}
