using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Events;


/// <summary>
/// Event component fired when two rigid bodies collide.
/// This component is automatically added by the physics system during collision
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to collisions:
/// <code>
/// foreach (var (entity, buffer) in world.Query&lt;CollisionEventBuffer&gt;())
/// {
///     for (int i = 0; i &lt; buffer.Count; ++i)
///         Console.WriteLine($"Collision with {buffer.GetEvent(i).OtherEntityId}");
/// }
/// </code>
/// </summary>
public struct CollisionEvent
{
    /// <summary>
    /// The entity ID that collided with this entity.
    /// </summary>
    public ulong OtherEntityId { get; set; }

    /// <summary>
    /// The contact point in world space where the collision occurred.
    /// </summary>
    public Vector3 ContactPoint { get; set; }

    /// <summary>
    /// The contact normal vector (points from this entity toward the other entity).
    /// </summary>
    public Vector3 ContactNormal { get; set; }

    /// <summary>
    /// The relative velocity at the contact point.
    /// </summary>
    public Vector3 RelativeVelocity { get; set; }

    /// <summary>
    /// The magnitude of the impulse applied to resolve the collision.
    /// </summary>
    public float ImpulseMagnitude { get; set; }
}
