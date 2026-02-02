using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
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
    private int _lastProcessedFrame = -1;
    private IntPtr _lastWorldPtr = IntPtr.Zero;
    private readonly List<ulong> _removeEnterBuffers = [];
    private readonly List<ulong> _removeExitBuffers = [];

    protected sealed override void OnUpdate()
    {
        var world = World!;

        unsafe
        {
            var worldPtr = (IntPtr)world.NativePtr;
            if (worldPtr != _lastWorldPtr)
            {
                _active.Clear();
                _lastProcessedFrame = -1;
                _lastWorldPtr = worldPtr;
            }
        }

        var frame = Time.FrameCount;
        if (_lastProcessedFrame == frame)
            return;
        _lastProcessedFrame = frame;
        var enterCount = 0;
        var exitCount = 0;
        var stayCount = 0;

        _removeEnterBuffers.Clear();
        _removeExitBuffers.Clear();

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
                    enterCount++;
                }
            }
            _removeEnterBuffers.Add(entity.Id);
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
                    exitCount++;
                }
            }
            _removeExitBuffers.Add(entity.Id);
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
                stayCount++;
            }
        }

        // Defensive: consume event buffers so stale data cannot re-enter.
        foreach (var entityId in _removeEnterBuffers)
        {
            var entity = Entity.FromId(world, entityId);
            if (entity.IsAlive && entity.HasComponent<CollisionEventBuffer>())
            {
                entity.RemoveComponent<CollisionEventBuffer>();
            }
        }
        foreach (var entityId in _removeExitBuffers)
        {
            var entity = Entity.FromId(world, entityId);
            if (entity.IsAlive && entity.HasComponent<CollisionExitEventBuffer>())
            {
                entity.RemoveComponent<CollisionExitEventBuffer>();
            }
        }

        if (kDebugEventLogging)
        {
            var collisionBuffers = 0;
            var collisionEvents = 0;
            var collisionExitBuffers = 0;
            var collisionExitEvents = 0;

            foreach (var (_, buffer) in world.Query<CollisionEventBuffer>())
            {
                collisionBuffers++;
                collisionEvents += buffer.Count;
            }
            foreach (var (_, buffer) in world.Query<CollisionExitEventBuffer>())
            {
                collisionExitBuffers++;
                collisionExitEvents += buffer.Count;
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
