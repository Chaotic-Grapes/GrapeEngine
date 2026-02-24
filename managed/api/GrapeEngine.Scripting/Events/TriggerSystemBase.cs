using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems.Attributes;

namespace GrapeEngine.Scripting.Events;

/// <summary>
/// Base class for trigger-driven systems that exposes Enter/Stay/Exit callbacks.
/// 
/// This wraps trigger event buffers emitted by the native physics system.
/// Use in PostPhysics to observe all trigger events for the frame.
/// </summary>
[System(SystemGroup.PostPhysics, SystemRunMode.PlayOnly)]
public abstract class TriggerSystemBase : SystemBase
{
    private readonly Dictionary<ulong, Dictionary<ulong, TriggerEvent>> _active = [];
    private int _lastProcessedFrame = -1;
    private IntPtr _lastWorldPtr = IntPtr.Zero;
    private const bool ClearBuffersAfterCache = true;
    private static int s_cachedFrame = -1;
    private static IntPtr s_cachedWorldPtr = IntPtr.Zero;
    private static readonly Dictionary<ulong, List<TriggerEvent>> s_cachedEnterEvents = [];
    private static readonly Dictionary<ulong, List<TriggerExitEvent>> s_cachedExitEvents = [];

    private static void RefreshFrameCache(World world, IntPtr worldPtr, int frame)
    {
        if (s_cachedFrame == frame && s_cachedWorldPtr == worldPtr)
            return;

        s_cachedFrame = frame;
        s_cachedWorldPtr = worldPtr;
        s_cachedEnterEvents.Clear();
        s_cachedExitEvents.Clear();

        foreach (var (entity, buffer) in world.Query<TriggerEventBuffer>())
        {
            if (buffer.Count <= 0)
                continue;

            var events = new List<TriggerEvent>(buffer.Count);
            for (var i = 0; i < buffer.Count; ++i)
            {
                events.Add(buffer.GetEvent(i));
            }
            s_cachedEnterEvents[entity.Id] = events;
        }

        foreach (var (entity, buffer) in world.Query<TriggerExitEventBuffer>())
        {
            if (buffer.Count <= 0)
                continue;

            var events = new List<TriggerExitEvent>(buffer.Count);
            for (var i = 0; i < buffer.Count; ++i)
            {
                events.Add(buffer.GetEvent(i));
            }
            s_cachedExitEvents[entity.Id] = events;
        }

        if (ClearBuffersAfterCache)
        {
            foreach (var (entity, _) in world.Query<TriggerEventBuffer>())
            {
                if (entity.IsAlive && entity.HasComponent<TriggerEventBuffer>())
                {
                    entity.RemoveComponent<TriggerEventBuffer>();
                }
            }
            foreach (var (entity, _) in world.Query<TriggerExitEventBuffer>())
            {
                if (entity.IsAlive && entity.HasComponent<TriggerExitEventBuffer>())
                {
                    entity.RemoveComponent<TriggerExitEventBuffer>();
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

        // Enter: TriggerEventBuffer contains enter events for the frame.
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
                if (evt.IsEnter && !map.ContainsKey(evt.OtherEntityId))
                {
                    map[evt.OtherEntityId] = evt;
                    OnTriggerEnter(entity, evt);
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
                    OnTriggerExit(entity, evt);
                    exitCount++;
                }
            }
        }

        // Stay: emit for all currently active overlaps.
        foreach (var kvp in _active)
        {
            var self = Entity.FromId(world, kvp.Key);
            if (!self.IsAlive)
                continue;

            // Process stay events
            foreach (var entry in kvp.Value)
            {
                OnTriggerStay(self, entry.Value);
                stayCount++;
            }
        }
    }

    /// <summary>
    /// Called when a trigger overlap begins.
    /// </summary>
    protected virtual void OnTriggerEnter(Entity self, TriggerEvent evt) { }

    /// <summary>
    /// Called each frame while a trigger overlap persists.
    /// </summary>
    protected virtual void OnTriggerStay(Entity self, TriggerEvent evt) { }

    /// <summary>
    /// Called when a trigger overlap ends.
    /// </summary>
    protected virtual void OnTriggerExit(Entity self, TriggerExitEvent evt) { }

}
