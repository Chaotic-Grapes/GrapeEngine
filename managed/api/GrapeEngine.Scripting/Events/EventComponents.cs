using GrapeEngine.Numerics;

namespace GrapeEngine.Scripting.Events;

/// <summary>
/// Event component fired when two rigid bodies collide.
/// This component is automatically added by the physics system during collision
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to collisions:
/// <code>
/// foreach (var (entity, collision) in world.Query&lt;CollisionEvent&gt;())
/// {
///     Console.WriteLine($"Collision with {collision.OtherEntityId}");
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

/// <summary>
/// Event component fired when a trigger/sensor overlaps another collider.
/// This component is automatically added by the physics system during trigger overlap
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to trigger events:
/// <code>
/// foreach (var (entity, trigger) in world.Query&lt;TriggerEvent&gt;())
/// {
///     if (trigger.IsEnter)
///         Console.WriteLine("Trigger entered!");
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

/// <summary>
/// Event component fired when a collision ends (two rigid bodies separate).
/// This component is automatically added by the physics system when collision ends
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to collision exit:
/// <code>
/// foreach (var (entity, exit) in world.Query&lt;CollisionExitEvent&gt;())
/// {
///     Console.WriteLine($"Stopped colliding with {exit.OtherEntityId}");
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

/// <summary>
/// Event component fired when a trigger stops overlapping another collider.
/// This component is automatically added by the physics system when trigger overlap ends
/// and persists only for the current frame. It's cleared at the end of each frame.
/// 
/// Query for this component to respond to trigger exit:
/// <code>
/// foreach (var (entity, exit) in world.Query&lt;TriggerExitEvent&gt;())
/// {
///     Console.WriteLine($"Trigger left by {exit.OtherEntityId}");
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

