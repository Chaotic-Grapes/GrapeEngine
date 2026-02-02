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

        // Enter: TriggerEventBuffer contains enter events for the frame.
        foreach (var (entity, buffer) in world.Query<TriggerEventBuffer>())
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
                if (evt.IsEnter && !map.ContainsKey(evt.OtherEntityId))
                {
                    map[evt.OtherEntityId] = evt;
                    OnTriggerEnter(entity, evt);
                    enterCount++;
                }
            }
        }

        // Exit: remove and notify.
        foreach (var (entity, buffer) in world.Query<TriggerExitEventBuffer>())
        {
            if (!_active.TryGetValue(entity.Id, out var map))
                continue;

            // Process exit events
            for (var i = 0; i < buffer.Count; ++i)
            {
                var evt = buffer.GetEvent(i);
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
