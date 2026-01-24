/* Start Header *****************************************************************/
/*!
\file   ComponentBatch.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Batched component access for reducing P/Invoke overhead.
Groups multiple component accesses to minimize transitions between managed and native code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using UnsafePtr = System.Runtime.CompilerServices.Unsafe;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Batches multiple component operations to reduce P/Invoke overhead.
/// Instead of accessing components one at a time, collect operations and execute them in a batch.
/// </summary>
/// <remarks>
/// Create a new component batch for the given world.
/// </remarks>
public class ComponentBatch(World world)
{
    /// <summary>
    /// Represents a queued read operation.
    /// </summary>
    private record ReadOperation(Entity Entity, uint ComponentTypeHash, Type ComponentType);

    /// <summary>
    /// Represents a queued write operation with untyped data.
    /// </summary>
    private record WriteOperation(Entity Entity, uint ComponentTypeHash, byte[] ComponentData, Type ComponentType);

    private readonly List<(ReadOperation? read, WriteOperation? write)> _operations = [];
    private readonly World _world = world;

    /// <summary>
    /// Queue a component read operation.
    /// </summary>
    public void QueueRead<T>(Entity entity) where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        var typeHash = ComponentTypeHelper.GetTypeHash<T>();
        var operation = new ReadOperation(entity, typeHash, typeof(T));
        _operations.Add((operation, null));
    }

    /// <summary>
    /// Queue a component write operation.
    /// </summary>
    public void QueueWrite<T>(Entity entity, T value) where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        var typeHash = ComponentTypeHelper.GetTypeHash<T>();

        // Marshal the unmanaged struct to bytes
        var size = Marshal.SizeOf<T>();
        var data = new byte[size];

        unsafe
        {
            fixed (byte* ptr = data)
            {
                Marshal.StructureToPtr(value, (IntPtr)ptr, false);
            }
        }

        var operation = new WriteOperation(entity, typeHash, data, typeof(T));
        _operations.Add((null, operation));
    }

    /// <summary>
    /// Execute all queued operations in a batch (reducing P/Invoke transitions).
    /// Returns results as a dictionary: (Entity, ComponentType) -> component value as object
    /// </summary>
    public Dictionary<(Entity, Type), object?> ExecuteBatch()
    {
        var results = new Dictionary<(Entity, Type), object?>();

        if (_operations.Count == 0)
        {
            return results;
        }

        // Process writes first
        foreach (var (_, write) in _operations)
        {
            if (write != null)
            {
                ProcessWrite(write, results);
            }
        }

        // Process reads
        foreach (var (read, _) in _operations)
        {
            if (read != null)
            {
                ProcessRead(read, results);
            }
        }

        return results;
    }

    /// <summary>
    /// Execute all queued operations and clear the batch.
    /// </summary>
    public Dictionary<(Entity, Type), object?> ExecuteAndClear()
    {
        var results = ExecuteBatch();
        Clear();
        return results;
    }

    /// <summary>
    /// Process a single read operation.
    /// </summary>
    private void ProcessRead(ReadOperation read, Dictionary<(Entity, Type), object?> results)
    {
        unsafe
        {
            if (read.Entity.IsAlive)
            {
                void* componentPtr = WorldAPI.GetComponentPtr(_world.NativePtr, read.Entity.Id, read.ComponentTypeHash);

                if (componentPtr != null)
                {
                    // Convert native pointer to managed object
                    object? value = Marshal.PtrToStructure(
                        (IntPtr)componentPtr,
                        read.ComponentType
                    );
                    results[(read.Entity, read.ComponentType)] = value;
                }
            }
        }
    }

    /// <summary>
    /// Process a single write operation.
    /// </summary>
    private void ProcessWrite(WriteOperation write, Dictionary<(Entity, Type), object?> results)
    {
        unsafe
        {
            if (write.Entity.IsAlive)
            {
                fixed (byte* dataPtr = write.ComponentData)
                {
                    void* addedPtr = WorldAPI.AddComponent(
                        _world.NativePtr,
                        write.Entity.Id,
                        write.ComponentTypeHash,
                        dataPtr,
                        write.ComponentData.Length
                    );

                    if (addedPtr != null)
                    {
                        // Store the result
                        object? value = Marshal.PtrToStructure(
                            (IntPtr)addedPtr,
                            write.ComponentType
                        );
                        results[(write.Entity, write.ComponentType)] = value;
                    }
                }
            }
        }
    }

    /// <summary>
    /// Clear all queued operations without executing them.
    /// </summary>
    public void Clear()
    {
        _operations.Clear();
    }

    /// <summary>
    /// Get the number of queued operations.
    /// </summary>
    public int Count => _operations.Count;
}

/// <summary>
/// Provides efficient batch access to entity components across multiple entities for a single component type T.
/// Optimized for high-volume component reads and writes with minimal P/Invoke overhead.
/// </summary>
public class EntityComponentBatchAccessor<T>(World world) where T : unmanaged
{
    private readonly World _world = world;
    private readonly uint _componentTypeHash = ComponentTypeHelper.GetTypeHash<T>();
    private readonly List<Entity> _entities = [];
    private readonly List<T> _values = [];

    /// <summary>
    /// Queue an entity to fetch component T from.
    /// </summary>
    public void QueueRead(Entity entity)
    {
        if (entity.IsAlive)
        {
            _entities.Add(entity);
        }
    }

    /// <summary>
    /// Fetch all queued components in a single batch operation.
    /// Returns a list of component values in the same order as queued.
    /// Only returns values for entities that have the component.
    /// </summary>
    public List<T> FetchBatch()
    {
        ComponentRegistry.EnsureRegistered<T>();
        _values.Clear();

        unsafe
        {
            foreach (var entity in _entities)
            {
                if (entity.IsAlive)
                {
                    void* componentPtr = WorldAPI.GetComponentPtr(_world.NativePtr, entity.Id, _componentTypeHash);

                    if (componentPtr != null)
                    {
                        _values.Add(*(T*)componentPtr);
                    }
                }
            }
        }

        return _values;
    }

    /// <summary>
    /// Fetch all queued components as references for in-place modification.
    /// Returns an array of tuples: (Entity, ref T component).
    /// </summary>
    public (Entity entity, T value)[] FetchBatchAsValues()
    {
        ComponentRegistry.EnsureRegistered<T>();
        var results = new (Entity, T)[_entities.Count];
        var resultIndex = 0;

        unsafe
        {
            foreach (var entity in _entities)
            {
                if (entity.IsAlive)
                {
                    void* componentPtr = WorldAPI.GetComponentPtr(_world.NativePtr, entity.Id, _componentTypeHash);

                    if (componentPtr != null)
                    {
                        results[resultIndex++] = (entity, *(T*)componentPtr);
                    }
                }
            }
        }

        // Trim to actual count
        System.Array.Resize(ref results, resultIndex);
        return results;
    }

    /// <summary>
    /// Write the same component value to all queued entities in one batch.
    /// Creates the component if it doesn't exist, overwrites if it does.
    /// </summary>
    public void WriteBatchSameValue(T value)
    {
        ComponentRegistry.EnsureRegistered<T>();
        int size = Marshal.SizeOf<T>();

        unsafe
        {
            T* valuePtr = (T*)UnsafePtr.AsPointer(ref value);

            foreach (var entity in _entities)
            {
                if (entity.IsAlive)
                {
                    if (entity.HasComponent<T>())
                    {
                        // Component exists, update it
                        ref var existing = ref entity.GetComponent<T>();
                        existing = value;
                    }
                    else
                    {
                        // Component doesn't exist, add it
                        WorldAPI.AddComponent(
                            _world.NativePtr,
                            entity.Id,
                            _componentTypeHash,
                            valuePtr,
                            size
                        );
                    }
                }
            }
        }
    }

    /// <summary>
    /// Write different component values to queued entities in one batch.
    /// Values list must match queued entities count.
    /// Creates components if they don't exist, overwrites if they do.
    /// </summary>
    public void WriteBatchValues(List<T> values)
    {
        if (values.Count != _entities.Count)
        {
            throw new ArgumentException(
                $"Values count ({values.Count}) must match entities count ({_entities.Count})"
            );
        }

        ComponentRegistry.EnsureRegistered<T>();
        var size = Marshal.SizeOf<T>();

        unsafe
        {
            for (var i = 0; i < _entities.Count; i++)
            {
                var entity = _entities[i];

                if (entity.IsAlive)
                {
                    T value = values[i];

                    if (entity.HasComponent<T>())
                    {
                        // Component exists, update it
                        ref var existing = ref entity.GetComponent<T>();
                        existing = value;
                    }
                    else
                    {
                        // Component doesn't exist, add it
                        T tempValue = value;
                        T* valuePtr = (T*)UnsafePtr.AsPointer(ref tempValue);
                        WorldAPI.AddComponent(
                            _world.NativePtr,
                            entity.Id,
                            _componentTypeHash,
                            valuePtr,
                            size
                        );
                    }
                }
            }
        }
    }

    /// <summary>
    /// Modify components for all queued entities using a delegate function.
    /// The delegate receives the current component value and returns the modified value.
    /// </summary>
    public void ModifyBatch(Func<T, T> modifier)
    {
        ComponentRegistry.EnsureRegistered<T>();

        foreach (var entity in _entities)
        {
            if (entity.IsAlive && entity.HasComponent<T>())
            {
                ref var component = ref entity.GetComponent<T>();
                component = modifier(component);
            }
        }
    }

    /// <summary>
    /// Process all queued entities with a callback for entities that have the component.
    /// </summary>
    public void ForEachComponent(Action<Entity, T> callback)
    {
        ComponentRegistry.EnsureRegistered<T>();

        unsafe
        {
            foreach (var entity in _entities)
            {
                if (entity.IsAlive)
                {
                    void* componentPtr = WorldAPI.GetComponentPtr(_world.NativePtr, entity.Id, _componentTypeHash);

                    if (componentPtr != null)
                    {
                        callback(entity, *(T*)componentPtr);
                    }
                }
            }
        }
    }

    /// <summary>
    /// Check how many queued entities actually have the component.
    /// </summary>
    public int CountHaving()
    {
        ComponentRegistry.EnsureRegistered<T>();
        var count = 0;

        unsafe
        {
            foreach (var entity in _entities)
            {
                if (entity.IsAlive && WorldAPI.HasComponent(_world.NativePtr, entity.Id, _componentTypeHash))
                {
                    count++;
                }
            }
        }

        return count;
    }

    /// <summary>
    /// Remove component from all queued entities that have it.
    /// </summary>
    public int RemoveFromAll()
    {
        ComponentRegistry.EnsureRegistered<T>();
        var removed = 0;

        unsafe
        {
            foreach (var entity in _entities)
            {
                if (entity.IsAlive && WorldAPI.HasComponent(_world.NativePtr, entity.Id, _componentTypeHash))
                {
                    WorldAPI.RemoveComponent(_world.NativePtr, entity.Id, _componentTypeHash);
                    removed++;
                }
            }
        }

        return removed;
    }

    /// <summary>
    /// Clear queued entities without fetching or modifying.
    /// </summary>
    public void Clear()
    {
        _entities.Clear();
        _values.Clear();
    }

    /// <summary>
    /// Get number of queued entities.
    /// </summary>
    public int Count => _entities.Count;
}

/// <summary>
/// Configuration for batch operation optimization.
/// </summary>
public static class BatchConfiguration
{
    /// <summary>
    /// Minimum number of operations to consider batching.
    /// If below this threshold, individual operations may be faster.
    /// </summary>
    public static int MinBatchSize { get; set; } = 10;

    /// <summary>
    /// Maximum operations to batch in a single call.
    /// Larger batches reduce P/Invoke overhead but increase latency per batch.
    /// </summary>
    public static int MaxBatchSize { get; set; } = 1000;

    /// <summary>
    /// Enable automatic batching for component access.
    /// </summary>
    public static bool AutoBatchingEnabled { get; set; } = true;
}

