using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace GrapeEngine.Scripting.Events;

/// <summary>
/// Base class for collision-driven systems that exposes Enter/Stay/Exit callbacks.
/// 
/// This wraps collision event components emitted by the native physics system.
/// Use in PostPhysics to observe all collision events for the frame.
/// </summary>
[System(SystemGroup.PostPhysics, SystemRunMode.PlayOnly)]
public abstract class CollisionSystemBase : SystemBase
{
    private readonly Dictionary<ulong, Dictionary<ulong, CollisionEvent>> _active = [];

    protected sealed override void OnUpdate()
    {
        var world = World!;

        // Enter: CollisionEvent is only emitted on new collision pairs.
        foreach (var (entity, buffer) in world.Query<CollisionEventBuffer>())
        {
            if (!_active.TryGetValue(entity.Id, out var map))
            {
                map = [];
                _active[entity.Id] = map;
            }

            // Process enter events
            for (var i = 0; i < buffer.Count; ++i)
            {
                var evt = buffer.GetEvent(i);

                // Only process enter events
                if (!map.ContainsKey(evt.OtherEntityId))
                {
                    map[evt.OtherEntityId] = evt;
                    OnCollisionEnter(entity, evt);
                }
            }
        }

        // Exit: remove and notify.
        foreach (var (entity, buffer) in world.Query<CollisionExitEventBuffer>())
        {
            if (!_active.TryGetValue(entity.Id, out var map))
                continue;

            for (var i = 0; i < buffer.Count; ++i)
            {
                var evt = buffer.GetEvent(i);
                if (map.Remove(evt.OtherEntityId))
                {
                    OnCollisionExit(entity, evt);
                }
            }
        }

        // Stay: emit for all currently active pairs.
        foreach (var kvp in _active)
        {
            var self = Entity.FromId(world, kvp.Key);
            if (!self.IsAlive)
                continue;

            foreach (var entry in kvp.Value)
            {
                OnCollisionStay(self, entry.Value);
            }
        }
    }

    /// <summary>
    /// Called when a collision begins.
    /// </summary>
    protected virtual void OnCollisionEnter(Entity self, CollisionEvent evt) { }

    /// <summary>
    /// Called each frame while a collision persists.
    /// </summary>
    protected virtual void OnCollisionStay(Entity self, CollisionEvent evt) { }

    /// <summary>
    /// Called when a collision ends.
    /// </summary>
    protected virtual void OnCollisionExit(Entity self, CollisionExitEvent evt) { }
}
