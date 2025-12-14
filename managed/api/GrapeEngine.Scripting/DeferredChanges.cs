/* Start Header *****************************************************************/
/*!
\file   DeferredChanges.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Infrastructure for deferred structural changes with change types and handlers.

Provides type-safe recording and playback of entity/component modifications
that occur during parallel job execution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Job;
using System.Diagnostics;

namespace GrapeEngine.Scripting;

/// <summary>
/// Base interface for deferred change handlers.
/// </summary>
public interface IChangeHandler
{
    /// <summary>
    /// Apply the change to the world
    /// </summary>
    void Apply(World world);

    /// <summary>
    /// Get human-readable description
    /// </summary>
    string GetDescription();
}

/// <summary>
/// Deferred entity creation change.
/// </summary>
public class CreateEntityChange : IChangeHandler
{
    /// <summary>
    /// Archetype for new entity (optional)
    /// </summary>
    public uint? ArchetypeId { get; set; }

    /// <summary>
    /// Initial components to add (optional)
    /// </summary>
    public Dictionary<Type, object> InitialComponents { get; set; } = [];

    public void Apply(World world)
    {
        var entity = world.CreateEntity();

        if (InitialComponents != null)
        {
            foreach (var (componentType, componentData) in InitialComponents)
            {
                try
                {
                    var method = typeof(World).GetMethod("AddComponent")
                        ?.MakeGenericMethod(componentType);

                    if (method != null)
                    {
                        method.Invoke(world, [entity, componentData]);
                    }
                    else
                    {
                        Console.WriteLine($"[DeferredChanges] CreateEntity failed: Could not find AddComponent method for {componentType.Name}");
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[DeferredChanges] CreateEntity error adding {componentType.Name}: {ex.Message}");
                }
            }
        }
    }

    public string GetDescription() => $"CreateEntity(archetype={ArchetypeId ?? 0}, components={InitialComponents?.Count ?? 0})";
}

/// <summary>
/// Deferred entity destruction change.
/// </summary>
public class DestroyEntityChange : IChangeHandler
{
    /// <summary>
    /// Entity to destroy
    /// </summary>
    public required Entity TargetEntity { get; set; }

    public void Apply(World world)
    {
        world.DestroyEntity(TargetEntity);
    }

    public string GetDescription() => $"DestroyEntity({TargetEntity.Id})";
}

/// <summary>
/// Deferred component addition change.
/// </summary>
public class AddComponentChange : IChangeHandler
{
    /// <summary>
    /// Entity to add component to
    /// </summary>
    public required Entity TargetEntity { get; set; }

    /// <summary>
    /// Component type
    /// </summary>
    public required Type ComponentType { get; set; }

    /// <summary>
    /// Component type name (for description only)
    /// </summary>
    public string ComponentTypeName { get; set; } = string.Empty;

    /// <summary>
    /// Component data
    /// </summary>
    public required object ComponentData { get; set; }

    public void Apply(World world)
    {
        if (ComponentType == null || ComponentData == null)
        {
            Console.WriteLine("[DeferredChanges] AddComponent failed: Missing component type or data");
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("AddComponent")
                ?.MakeGenericMethod(ComponentType);

            if (method != null)
            {
                method.Invoke(world, [TargetEntity, ComponentData]);
            }
            else
            {
                Console.WriteLine($"[DeferredChanges] AddComponent failed: Could not find method for {ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[DeferredChanges] AddComponent error: {ex.Message}");
        }
    }

    public string GetDescription() => $"AddComponent<{ComponentTypeName ?? ComponentType?.Name ?? "Unknown"}>({TargetEntity.Id})";
}

/// <summary>
/// Deferred component removal change.
/// </summary>
public class RemoveComponentChange : IChangeHandler
{
    /// <summary>
    /// Entity to remove component from
    /// </summary>
    public required Entity TargetEntity { get; set; }

    /// <summary>
    /// Component type
    /// </summary>
    public required Type ComponentType { get; set; }

    /// <summary>
    /// Component type name (for description only)
    /// </summary>
    public string ComponentTypeName { get; set; } = string.Empty;

    public void Apply(World world)
    {
        if (ComponentType == null)
        {
            Console.WriteLine("[DeferredChanges] RemoveComponent failed: Missing component type");
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("RemoveComponent")
                ?.MakeGenericMethod(ComponentType);

            if (method != null)
            {
                method.Invoke(world, [TargetEntity]);
            }
            else
            {
                Console.WriteLine(
                    $"[DeferredChanges] RemoveComponent failed: Could not find method for {ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                $"[DeferredChanges] RemoveComponent error: {ex.Message}");
        }
    }

    public string GetDescription() => $"RemoveComponent<{ComponentTypeName ?? ComponentType?.Name ?? "Unknown"}>({TargetEntity.Id})";
}

/// <summary>
/// Deferred component update change.
/// </summary>
public class SetComponentChange : IChangeHandler
{
    /// <summary>
    /// Entity to set component on
    /// </summary>
    public required Entity TargetEntity { get; set; }

    /// <summary>
    /// Component type
    /// </summary>
    public required Type ComponentType { get; set; }

    /// <summary>
    /// Component type name (for description only)
    /// </summary>
    public string ComponentTypeName { get; set; } = string.Empty;

    /// <summary>
    /// Component data
    /// </summary>
    public required object ComponentData { get; set; }

    public void Apply(World world)
    {
        if (ComponentType == null || ComponentData == null)
        {
            Console.WriteLine(
                "[DeferredChanges] SetComponent failed: Missing component type or data");
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("SetComponent")
                ?.MakeGenericMethod(ComponentType);

            if (method != null)
            {
                method.Invoke(world, [TargetEntity, ComponentData]);
            }
            else
            {
                Console.WriteLine(
                    $"[DeferredChanges] SetComponent failed: Could not find method for {ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                $"[DeferredChanges] SetComponent error: {ex.Message}");
        }
    }

    public string GetDescription() => $"SetComponent<{ComponentTypeName ?? ComponentType?.Name ?? "Unknown"}>({TargetEntity.Id})";
}

/// <summary>
/// Queue of deferred changes to apply at a synchronization point.
/// 
/// Records changes as they occur and applies them in order
/// at a safe point to prevent data races.
/// </summary>
public class DeferredChangeQueue
{
    private readonly Queue<IChangeHandler> _changes;
    private readonly Stopwatch _profiler;
    private bool _isProcessing;

    /// <summary>
    /// Create a deferred change queue.
    /// </summary>
    public DeferredChangeQueue()
    {
        _changes = new Queue<IChangeHandler>();
        _profiler = new Stopwatch();
        _isProcessing = false;
    }

    /// <summary>
    /// Enqueue a change to be applied later.
    /// </summary>
    /// <param name="change">Change to enqueue</param>
    public void Enqueue(IChangeHandler change)
    {
        ArgumentNullException.ThrowIfNull(change);

        _changes.Enqueue(change);
    }

    /// <summary>
    /// Apply all queued changes to the world.
    /// </summary>
    /// <param name="world">World to apply changes to</param>
    /// <returns>Number of changes applied</returns>
    public int ApplyChanges(World world)
    {
        ArgumentNullException.ThrowIfNull(world);

        if (_isProcessing)
            throw new InvalidOperationException("Already processing changes");

        var changeCount = _changes.Count;

        if (changeCount == 0)
            return 0;

        _isProcessing = true;
        _profiler.Restart();

        try
        {
            while (_changes.Count > 0)
            {
                var change = _changes.Dequeue();
                change.Apply(world);
            }
        }
        finally
        {
            _profiler.Stop();
            _isProcessing = false;
        }

        return changeCount;
    }

    /// <summary>
    /// Get count of pending changes.
    /// </summary>
    public int GetPendingChangeCount() => _changes.Count;

    /// <summary>
    /// Clear all pending changes without applying.
    /// </summary>
    public void Clear()
    {
        _changes.Clear();
    }

    /// <summary>
    /// Get time spent applying changes.
    /// </summary>
    public long GetLastProcessingTimeMs()
    {
        return _profiler.ElapsedMilliseconds;
    }
}

/// <summary>
/// Coordinator for deferred changes across multiple sources.
/// 
/// Manages change requests from parallel jobs, maintaining order
/// and applying them safely at synchronization points.
/// </summary>
/// <remarks>
/// Create a deferred change coordinator.
/// </remarks>
public class DeferredChangeCoordinator(World world)
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));
    private readonly List<DeferredChangeQueue> _queues = [];
    private readonly Lock _lock = new();

  /// <summary>
  /// Create a new change queue (thread-safe).
  /// </summary>
  /// <returns>New change queue</returns>
  public DeferredChangeQueue CreateQueue()
    {
        var queue = new DeferredChangeQueue();

        lock (_lock)
        {
            _queues.Add(queue);
        }

        return queue;
    }

    /// <summary>
    /// Apply all changes from all queues in order.
    /// </summary>
    /// <returns>Total changes applied</returns>
    public int ApplyAllChanges()
    {
        var totalApplied = 0;

        lock (_lock)
        {
            foreach (var queue in _queues)
            {
                totalApplied += queue.ApplyChanges(_world);
            }
        }

        return totalApplied;
    }

    /// <summary>
    /// Get total pending changes across all queues.
    /// </summary>
    public int GetTotalPendingChanges()
    {
        var total = 0;

        lock (_lock)
        {
            foreach (var queue in _queues)
            {
                total += queue.GetPendingChangeCount();
            }
        }

        return total;
    }

    /// <summary>
    /// Clear all queues.
    /// </summary>
    public void ClearAll()
    {
        lock (_lock)
        {
            foreach (var queue in _queues)
            {
                queue.Clear();
            }
        }
    }

    /// <summary>
    /// Get queue count.
    /// </summary>
    public int GetQueueCount()
    {
        lock (_lock)
        {
            return _queues.Count;
        }
    }
}

/// <summary>
/// Synchronization barrier for coordinating deferred changes.
/// 
/// Ensures that:
/// 1. All jobs complete before changes are applied
/// 2. Changes are applied before next frame starts
/// 3. No new changes are recorded during playback
/// </summary>
/// <remarks>
/// Create a synchronization barrier.
/// </remarks>
public class ChangeSynchronizationBarrier(World world)
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));
    private readonly CommandBufferManager _bufferManager = new(world);
    private readonly DeferredChangeCoordinator _changeCoordinator = new(world);
    private JobHandle? _pendingHandle;
    private bool _isOpen = true;

  /// <summary>
  /// Register a pending job handle.
  /// </summary>
  public void RegisterPendingJob(JobHandle handle)
    {
        _pendingHandle = handle;
    }

    /// <summary>
    /// Synchronize: wait for all jobs and apply changes.
    /// </summary>
    /// <returns>Number of changes applied</returns>
    public int Synchronize()
    {
        if (!_isOpen)
            throw new InvalidOperationException("Barrier is closed");

        // Wait for pending jobs
        if (_pendingHandle != null && _pendingHandle.IsValid)
        {
            _pendingHandle.Complete();
        }

        // Apply command buffer changes
        var bufferCommands = _bufferManager.PlaybackAll();

        // Apply deferred changes
        var deferredChanges = _changeCoordinator.ApplyAllChanges();

        return bufferCommands + deferredChanges;
    }

    /// <summary>
    /// Get a command buffer for recording changes.
    /// </summary>
    public CommandBuffer GetCommandBuffer(string name = "default")
    {
        return _bufferManager.GetOrCreateBuffer(name);
    }

    /// <summary>
    /// Get a change queue for recording deferred changes.
    /// </summary>
    public DeferredChangeQueue GetChangeQueue()
    {
        return _changeCoordinator.CreateQueue();
    }

    /// <summary>
    /// Open the barrier for recording changes.
    /// </summary>
    public void Open()
    {
        _isOpen = true;
    }

    /// <summary>
    /// Close the barrier (prevents new changes).
    /// </summary>
    public void Close()
    {
        _isOpen = false;
    }

    /// <summary>
    /// Check if barrier is open.
    /// </summary>
    public bool IsOpen => _isOpen;

    /// <summary>
    /// Reset for next frame.
    /// </summary>
    public void Reset()
    {
        _bufferManager.ClearAll();
        _changeCoordinator.ClearAll();
        _pendingHandle = JobHandle.CreateInvalid();
        Open();
    }
}

/// <summary>
/// Frame-level manager for structural changes.
/// 
/// Orchestrates command buffers and deferred changes across an entire frame,
/// ensuring safe and ordered application of structural modifications.
/// </summary>
/// <remarks>
/// Create a frame change manager.
/// </remarks>
public class FrameChangeManager(World world)
{
    private readonly World _world = world ?? throw new ArgumentNullException(nameof(world));
    private readonly ChangeSynchronizationBarrier _barrier = new(world);
    private uint _currentFrame = 0;

  /// <summary>
  /// Begin a frame.
  /// </summary>
  public void BeginFrame()
    {
        _barrier.Open();
    }

    /// <summary>
    /// End a frame and apply all deferred changes.
    /// </summary>
    /// <returns>Number of changes applied</returns>
    public int EndFrame()
    {
        var applied = _barrier.Synchronize();
        _barrier.Reset();
        _currentFrame++;

        return applied;
    }

    /// <summary>
    /// Get command buffer for current frame.
    /// </summary>
    public CommandBuffer GetCommandBuffer(string name = "default")
    {
        return _barrier.GetCommandBuffer(name);
    }

    /// <summary>
    /// Get change queue for current frame.
    /// </summary>
    public DeferredChangeQueue GetChangeQueue()
    {
        return _barrier.GetChangeQueue();
    }

    /// <summary>
    /// Register a job to wait for before applying changes.
    /// </summary>
    public void RegisterJobHandle(JobHandle handle)
    {
        _barrier.RegisterPendingJob(handle);
    }

    /// <summary>
    /// Get current frame number.
    /// </summary>
    public uint GetCurrentFrame() => _currentFrame;

    /// <summary>
    /// Get total pending changes.
    /// </summary>
    public int GetPendingChangeCount()
    {
        return _barrier.GetCommandBuffer().GetPendingCommandCount();
    }
}
