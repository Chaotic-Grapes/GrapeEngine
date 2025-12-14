using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Events;

/// <summary>
/// Provides ergonomic access to physics event components.
/// 
/// The C++ EventDispatcher automatically adds CollisionEvent, TriggerEvent, and 
/// CollisionExitEvent components to entities when physics events occur.
/// This EventSystem provides convenience methods to query for these events.
/// 
/// Event components are automatically removed at the end of each frame by the C++ ECS.
/// </summary>
/// <remarks>
/// This is a thin wrapper around component queries. You can also query events directly:
/// <code>
/// foreach (var (entity, collision) in world.Query&lt;CollisionEvent&gt;())
/// {
///     HandleCollision(entity, collision);
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
        return entity.IsAlive && entity.HasComponent<CollisionEvent>();
    }

    /// <summary>
    /// Get the collision event details for an entity (if any).
    /// Returns null if the entity has no collision event this frame.
    /// </summary>
    public CollisionEvent? GetCollisionEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (entity.IsAlive && entity.HasComponent<CollisionEvent>())
        {
            return entity.GetComponent<CollisionEvent>();
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
        return entity.IsAlive && entity.HasComponent<TriggerEvent>();
    }

    /// <summary>
    /// Get the trigger event details for an entity (if any).
    /// Returns null if the entity has no trigger event this frame.
    /// </summary>
    public TriggerEvent? GetTriggerEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (entity.IsAlive && entity.HasComponent<TriggerEvent>())
        {
            return entity.GetComponent<TriggerEvent>();
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
        return entity.IsAlive && entity.HasComponent<CollisionExitEvent>();
    }

    /// <summary>
    /// Get the collision exit event details for an entity (if any).
    /// Returns null if the entity has no collision exit event this frame.
    /// </summary>
    public CollisionExitEvent? GetCollisionExitEvent(ulong entityId)
    {
        var entity = Entity.FromId(_world, entityId);
        if (entity.IsAlive && entity.HasComponent<CollisionExitEvent>())
        {
            return entity.GetComponent<CollisionExitEvent>();
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
            foreach (var _ in _world.Query<CollisionEvent>())
            {
                count++;
            }
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
            foreach (var _ in _world.Query<TriggerEvent>())
            {
                count++;
            }
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
            foreach (var _ in _world.Query<CollisionExitEvent>())
            {
                count++;
            }
            return count;
        }
    }

    /// <summary>
    /// Get all collision events that occurred this frame.
    /// </summary>
    public IEnumerable<(Entity Entity, CollisionEvent Event)> GetAllCollisionEvents()
    {
        foreach (var (entity, collision) in _world.Query<CollisionEvent>())
        {
            yield return (entity, collision);
        }
    }

    /// <summary>
    /// Get all trigger events that occurred this frame.
    /// </summary>
    public IEnumerable<(Entity Entity, TriggerEvent Event)> GetAllTriggerEvents()
    {
        foreach (var (entity, trigger) in _world.Query<TriggerEvent>())
        {
            yield return (entity, trigger);
        }
    }

    /// <summary>
    /// Get all collision exit events that occurred this frame.
    /// </summary>
    public IEnumerable<(Entity Entity, CollisionExitEvent Event)> GetAllCollisionExitEvents()
    {
        foreach (var (entity, exit) in _world.Query<CollisionExitEvent>())
        {
            yield return (entity, exit);
        }
    }
}

