using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Events;

/// <summary>
/// Manages event component lifecycle and provides utilities for working with events.
/// This system is responsible for tracking event components and clearing them at appropriate times.
/// </summary>
/// <remarks>
/// Initialize the event system with a world reference.
/// </remarks>
public class EventSystem(World world)
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));
    private readonly HashSet<ulong> _activeCollisionEvents = [];
    private readonly HashSet<ulong> _activeTriggerEvents = [];
    private readonly HashSet<ulong> _activeCollisionExitEvents = [];
    private readonly HashSet<ulong> _activeTriggerExitEvents = [];

    /// <summary>
    /// Track a collision event that occurred this frame.
    /// Typically called by the physics system.
    /// </summary>
    public void TrackCollisionEvent(ulong entityId)
    {
        _activeCollisionEvents.Add(entityId);
    }

    /// <summary>
    /// Track a trigger event that occurred this frame.
    /// Typically called by the physics system.
    /// </summary>
    public void TrackTriggerEvent(ulong entityId)
    {
        _activeTriggerEvents.Add(entityId);
    }

    /// <summary>
    /// Track a collision exit event that occurred this frame.
    /// Typically called by the physics system.
    /// </summary>
    public void TrackCollisionExitEvent(ulong entityId)
    {
        _activeCollisionExitEvents.Add(entityId);
    }

    /// <summary>
    /// Track a trigger exit event that occurred this frame.
    /// Typically called by the physics system.
    /// </summary>
    public void TrackTriggerExitEvent(ulong entityId)
    {
        _activeTriggerExitEvents.Add(entityId);
    }

    /// <summary>
    /// Check if an entity has a collision event this frame.
    /// </summary>
    public bool HasCollisionEvent(ulong entityId)
    {
        return _activeCollisionEvents.Contains(entityId);
    }

    /// <summary>
    /// Check if an entity has a trigger event this frame.
    /// </summary>
    public bool HasTriggerEvent(ulong entityId)
    {
        return _activeTriggerEvents.Contains(entityId);
    }

    /// <summary>
    /// Check if an entity has a collision exit event this frame.
    /// </summary>
    public bool HasCollisionExitEvent(ulong entityId)
    {
        return _activeCollisionExitEvents.Contains(entityId);
    }

    /// <summary>
    /// Check if an entity has a trigger exit event this frame.
    /// </summary>
    public bool HasTriggerExitEvent(ulong entityId)
    {
        return _activeTriggerExitEvents.Contains(entityId);
    }

    /// <summary>
    /// Called at the end of each frame to clear all event components.
    /// This ensures events only persist for a single frame.
    /// </summary>
    public void ClearFrameEvents()
    {
        // Clear collision events
        foreach (ulong entityId in _activeCollisionEvents)
        {
            var entity = Entity.FromId(_world, entityId);
            if (entity.IsAlive && entity.HasComponent<CollisionEvent>())
            {
                entity.RemoveComponent<CollisionEvent>();
            }
        }
        _activeCollisionEvents.Clear();

        // Clear trigger events
        foreach (ulong entityId in _activeTriggerEvents)
        {
            var entity = Entity.FromId(_world, entityId);
            if (entity.IsAlive && entity.HasComponent<TriggerEvent>())
            {
                entity.RemoveComponent<TriggerEvent>();
            }
        }
        _activeTriggerEvents.Clear();

        // Clear collision exit events
        foreach (ulong entityId in _activeCollisionExitEvents)
        {
            var entity = Entity.FromId(_world, entityId);
            if (entity.IsAlive && entity.HasComponent<CollisionExitEvent>())
            {
                entity.RemoveComponent<CollisionExitEvent>();
            }
        }
        _activeCollisionExitEvents.Clear();

        // Clear trigger exit events
        foreach (ulong entityId in _activeTriggerExitEvents)
        {
            var entity = Entity.FromId(_world, entityId);
            if (entity.IsAlive && entity.HasComponent<TriggerExitEvent>())
            {
                entity.RemoveComponent<TriggerExitEvent>();
            }
        }
        _activeTriggerExitEvents.Clear();
    }

    /// <summary>
    /// Get the count of active collision events this frame.
    /// </summary>
    public int CollisionEventCount => _activeCollisionEvents.Count;

    /// <summary>
    /// Get the count of active trigger events this frame.
    /// </summary>
    public int TriggerEventCount => _activeTriggerEvents.Count;

    /// <summary>
    /// Get the count of active collision exit events this frame.
    /// </summary>
    public int CollisionExitEventCount => _activeCollisionExitEvents.Count;

    /// <summary>
    /// Get the count of active trigger exit events this frame.
    /// </summary>
    public int TriggerExitEventCount => _activeTriggerExitEvents.Count;
}

