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

/// <summary>
/// Buffer of collision events for a single entity in the current frame.
/// </summary>
public struct CollisionEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public CollisionEvent Event0;
    public CollisionEvent Event1;
    public CollisionEvent Event2;
    public CollisionEvent Event3;
    public CollisionEvent Event4;
    public CollisionEvent Event5;
    public CollisionEvent Event6;
    public CollisionEvent Event7;

    public CollisionEvent GetEvent(int index)
    {
        return index switch
        {
            0 => Event0,
            1 => Event1,
            2 => Event2,
            3 => Event3,
            4 => Event4,
            5 => Event5,
            6 => Event6,
            7 => Event7,
            _ => default
        };
    }
}

/// <summary>
/// Buffer of trigger events for a single entity in the current frame.
/// </summary>
public struct TriggerEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public TriggerEvent Event0;
    public TriggerEvent Event1;
    public TriggerEvent Event2;
    public TriggerEvent Event3;
    public TriggerEvent Event4;
    public TriggerEvent Event5;
    public TriggerEvent Event6;
    public TriggerEvent Event7;

    public TriggerEvent GetEvent(int index)
    {
        return index switch
        {
            0 => Event0,
            1 => Event1,
            2 => Event2,
            3 => Event3,
            4 => Event4,
            5 => Event5,
            6 => Event6,
            7 => Event7,
            _ => default
        };
    }
}

/// <summary>
/// Buffer of collision exit events for a single entity in the current frame.
/// </summary>
public struct CollisionExitEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public CollisionExitEvent Event0;
    public CollisionExitEvent Event1;
    public CollisionExitEvent Event2;
    public CollisionExitEvent Event3;
    public CollisionExitEvent Event4;
    public CollisionExitEvent Event5;
    public CollisionExitEvent Event6;
    public CollisionExitEvent Event7;

    public CollisionExitEvent GetEvent(int index)
    {
        return index switch
        {
            0 => Event0,
            1 => Event1,
            2 => Event2,
            3 => Event3,
            4 => Event4,
            5 => Event5,
            6 => Event6,
            7 => Event7,
            _ => default
        };
    }
}

/// <summary>
/// Buffer of trigger exit events for a single entity in the current frame.
/// </summary>
public struct TriggerExitEventBuffer
{
    public const int MaxEvents = 8;
    public int Count;
    public TriggerExitEvent Event0;
    public TriggerExitEvent Event1;
    public TriggerExitEvent Event2;
    public TriggerExitEvent Event3;
    public TriggerExitEvent Event4;
    public TriggerExitEvent Event5;
    public TriggerExitEvent Event6;
    public TriggerExitEvent Event7;

    public TriggerExitEvent GetEvent(int index)
    {
        return index switch
        {
            0 => Event0,
            1 => Event1,
            2 => Event2,
            3 => Event3,
            4 => Event4,
            5 => Event5,
            6 => Event6,
            7 => Event7,
            _ => default
        };
    }
}


