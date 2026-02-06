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
    private const bool ClearBuffersAfterCache = true;
    private static int s_cachedFrame = -1;
    private static IntPtr s_cachedWorldPtr = IntPtr.Zero;
    private static readonly Dictionary<ulong, List<CollisionEvent>> s_cachedEnterEvents = [];
    private static readonly Dictionary<ulong, List<CollisionExitEvent>> s_cachedExitEvents = [];

    private static void RefreshFrameCache(World world, IntPtr worldPtr, int frame)
    {
        if (s_cachedFrame == frame && s_cachedWorldPtr == worldPtr)
            return;

        s_cachedFrame = frame;
        s_cachedWorldPtr = worldPtr;
        s_cachedEnterEvents.Clear();
        s_cachedExitEvents.Clear();

        // Cache enter events
        foreach (var (entity, buffer) in world.Query<CollisionEventBuffer>())
        {
            if (buffer.Count <= 0)
                continue;

            var events = new List<CollisionEvent>(buffer.Count);
            for (var i = 0; i < buffer.Count; ++i)
            {
                events.Add(buffer.GetEvent(i));
            }
            s_cachedEnterEvents[entity.Id] = events;
        }

        // Exit events
        foreach (var (entity, buffer) in world.Query<CollisionExitEventBuffer>())
        {
            if (buffer.Count <= 0)
                continue;

            var events = new List<CollisionExitEvent>(buffer.Count);
            for (var i = 0; i < buffer.Count; ++i)
            {
                events.Add(buffer.GetEvent(i));
            }
            s_cachedExitEvents[entity.Id] = events;
        }

        // Clear buffers to avoid double-processing
        if (ClearBuffersAfterCache)
        {
            foreach (var (entity, _) in world.Query<CollisionEventBuffer>())
            {
                if (entity.IsAlive && entity.HasComponent<CollisionEventBuffer>())
                {
                    entity.RemoveComponent<CollisionEventBuffer>();
                }
            }
            foreach (var (entity, _) in world.Query<CollisionExitEventBuffer>())
            {
                if (entity.IsAlive && entity.HasComponent<CollisionExitEventBuffer>())
                {
                    entity.RemoveComponent<CollisionExitEventBuffer>();
                }
            }
        }
    }

    protected sealed override void OnUpdate()
    {
        var world = World!;

        IntPtr worldPtr;
        unsafe
        {
            worldPtr = (IntPtr)world.NativePtr;
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

        RefreshFrameCache(world, worldPtr, frame);

        // Enter: CollisionEvent is only emitted on new collision pairs.
        foreach (var (entityId, events) in s_cachedEnterEvents)
        {
            var entity = Entity.FromId(world, entityId);
            if (!entity.IsAlive)
                continue;

            if (!_active.TryGetValue(entity.Id, out var map))
            {
                map = [];
                _active[entity.Id] = map;
            }

            for (var i = 0; i < events.Count; ++i)
            {
                var evt = events[i];
                if (!map.ContainsKey(evt.OtherEntityId))
                {
                    map[evt.OtherEntityId] = evt;
                    OnCollisionEnter(entity, evt);
                    enterCount++;
                }
            }
        }

        // Exit: remove and notify.
        foreach (var (entityId, events) in s_cachedExitEvents)
        {
            var entity = Entity.FromId(world, entityId);
            if (!entity.IsAlive)
                continue;

            if (!_active.TryGetValue(entity.Id, out var map))
                continue;

            for (var i = 0; i < events.Count; ++i)
            {
                var evt = events[i];
                if (map.Remove(evt.OtherEntityId))
                {
                    OnCollisionExit(entity, evt);
                    exitCount++;
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
                stayCount++;
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
