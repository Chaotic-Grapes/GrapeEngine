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

