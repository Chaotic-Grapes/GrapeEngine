using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Events;

/// <summary>
/// Provides ergonomic access to physics event components.
/// 
/// The C++ EventDispatcher automatically adds per-entity event buffers when physics events occur.
/// This EventSystem provides convenience methods to query for these events.
/// 
/// Event components are automatically removed at the end of each frame by the C++ ECS.
/// </summary>
/// <remarks>
/// This is a thin wrapper around component queries. You can also query events directly:
/// <code>
/// foreach (var (entity, buffer) in world.Query&lt;CollisionEventBuffer&gt;())
/// {
///     for (int i = 0; i &lt; buffer.Count; ++i)
///         HandleCollision(entity, buffer.GetEvent(i));
/// }
/// </code>
/// </remarks>
public class EventSystem(World world)
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));

    /// <summary>
    /// Check if an entity has a collision event this frame.
    /// 
    /// Events are added by the C++ physics system when collisions occur
    /// and are automatically removed at the end of each frame.
    /// </summary>
    public bool HasCollisionEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (!entity.IsAlive || !entity.HasComponent<CollisionEventBuffer>())
            return false;
        return entity.GetComponent<CollisionEventBuffer>().Count > 0;
    }

    /// <summary>
    /// Get the collision event details for an entity (if any).
    /// Returns null if the entity has no collision event this frame.
    /// </summary>
    public CollisionEvent? GetCollisionEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (entity.IsAlive && entity.HasComponent<CollisionEventBuffer>())
        {
            var buffer = entity.GetComponent<CollisionEventBuffer>();
            return buffer.Count > 0 ? buffer.GetEvent(0) : null;
        }
        return null;
    }

    /// <summary>
    /// Check if an entity has a trigger event this frame.
    /// 
    /// Trigger events are added when a trigger collider overlaps another object.
    /// Use TriggerEvent.IsEnter to distinguish between enter and stay events.
    /// </summary>
    public bool HasTriggerEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (!entity.IsAlive || !entity.HasComponent<TriggerEventBuffer>())
            return false;
        return entity.GetComponent<TriggerEventBuffer>().Count > 0;
    }

    /// <summary>
    /// Get the trigger event details for an entity (if any).
    /// Returns null if the entity has no trigger event this frame.
    /// </summary>
    public TriggerEvent? GetTriggerEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (entity.IsAlive && entity.HasComponent<TriggerEventBuffer>())
        {
            var buffer = entity.GetComponent<TriggerEventBuffer>();
            return buffer.Count > 0 ? buffer.GetEvent(0) : null;
        }
        return null;
    }

    /// <summary>
    /// Check if an entity has a collision exit event this frame.
    /// 
    /// Collision exit events occur when two colliding entities separate.
    /// </summary>
    public bool HasCollisionExitEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (!entity.IsAlive || !entity.HasComponent<CollisionExitEventBuffer>())
            return false;
        return entity.GetComponent<CollisionExitEventBuffer>().Count > 0;
    }

    /// <summary>
    /// Get the collision exit event details for an entity (if any).
    /// Returns null if the entity has no collision exit event this frame.
    /// </summary>
    public CollisionExitEvent? GetCollisionExitEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (entity.IsAlive && entity.HasComponent<CollisionExitEventBuffer>())
        {
            var buffer = entity.GetComponent<CollisionExitEventBuffer>();
            return buffer.Count > 0 ? buffer.GetEvent(0) : null;
        }
        return null;
    }

    /// <summary>
    /// Get the count of collision events this frame (all entities with CollisionEvent).
    /// </summary>
    public int CollisionEventCount
    {
        get
        {
            int count = 0;
            foreach (var (_, buffer) in _world.Query<CollisionEventBuffer>())
                count += buffer.Count;
            return count;
        }
    }

    /// <summary>
    /// Get the count of trigger events this frame (all entities with TriggerEvent).
    /// </summary>
    public int TriggerEventCount
    {
        get
        {
            int count = 0;
            foreach (var (_, buffer) in _world.Query<TriggerEventBuffer>())
                count += buffer.Count;
            return count;
        }
    }

    /// <summary>
    /// Get the count of collision exit events this frame.
    /// </summary>
    public int CollisionExitEventCount
    {
        get
        {
            int count = 0;
            foreach (var (_, buffer) in _world.Query<CollisionExitEventBuffer>())
                count += buffer.Count;
            return count;
        }
    }

    /// <summary>
    /// Get all collision events that occurred this frame.
    /// </summary>
    public IEnumerable<(Entity Entity, CollisionEvent Event)> GetAllCollisionEvents()
    {
        foreach (var (entity, buffer) in _world.Query<CollisionEventBuffer>())
        {
            for (int i = 0; i < buffer.Count; ++i)
                yield return (entity, buffer.GetEvent(i));
        }
    }

    /// <summary>
    /// Get all trigger events that occurred this frame.
    /// </summary>
    public IEnumerable<(Entity Entity, TriggerEvent Event)> GetAllTriggerEvents()
    {
        foreach (var (entity, buffer) in _world.Query<TriggerEventBuffer>())
        {
            for (int i = 0; i < buffer.Count; ++i)
                yield return (entity, buffer.GetEvent(i));
        }
    }

    /// <summary>
    /// Get all collision exit events that occurred this frame.
    /// </summary>
    public IEnumerable<(Entity Entity, CollisionExitEvent Event)> GetAllCollisionExitEvents()
    {
        foreach (var (entity, buffer) in _world.Query<CollisionExitEventBuffer>())
        {
            for (int i = 0; i < buffer.Count; ++i)
                yield return (entity, buffer.GetEvent(i));
        }
    }
}


