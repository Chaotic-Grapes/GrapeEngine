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

using System.Collections.Generic;

namespace GrapeEngine.Scripting;

/// <summary>
/// Batches multiple component operations to reduce P/Invoke overhead.
/// Instead of accessing components one at a time, collect operations and execute them in a batch.
/// </summary>
public class ComponentBatch
{
    private readonly List<(Entity entity, int componentTypeId, object? value, bool isRead)> _operations = [];
    private readonly World _world;

    /// <summary>
    /// Create a new component batch for the given world.
    /// </summary>
    public ComponentBatch(World world)
    {
        _world = world;
    }

    /// <summary>
    /// Queue a component read operation.
    /// </summary>
    public void QueueRead<T>(Entity entity) where T : unmanaged
    {
        var typeId = ComponentRegistry.GetComponentTypeId<T>();
        _operations.Add((entity, typeId, null, true));
    }

    /// <summary>
    /// Queue a component write operation.
    /// </summary>
    public void QueueWrite<T>(Entity entity, T value) where T : unmanaged
    {
        var typeId = ComponentRegistry.GetComponentTypeId<T>();
        _operations.Add((entity, typeId, value, false));
    }

    /// <summary>
    /// Execute all queued operations in a batch (reducing P/Invoke transitions).
    /// Returns results as a dictionary: (entity, componentTypeId) -> value
    /// </summary>
    public Dictionary<(Entity, int), object?> ExecuteBatch()
    {
        var results = new Dictionary<(Entity, int), object?>();
        
        // Group operations by entity to minimize native calls
        var operationsByEntity = _operations
            .GroupBy(op => op.entity)
            .ToDictionary(g => g.Key, g => g.ToList());

        foreach (var (entity, ops) in operationsByEntity)
        {
            foreach (var (_, componentTypeId, value, isRead) in ops)
            {
                if (isRead)
                {
                    // Read operation - would call into native code to get component
                    // This is a placeholder; actual implementation depends on ECS API
                    results[(entity, componentTypeId)] = null;
                }
                else
                {
                    // Write operation - would call into native code to set component
                    results[(entity, componentTypeId)] = value;
                }
            }
        }

        return results;
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
/// Provides efficient batch access to entity components across multiple entities.
/// </summary>
public class EntityComponentBatchAccessor<T> where T : unmanaged
{
    private readonly World _world;
    private readonly int _componentTypeId;
    private readonly List<Entity> _entities = [];
    private readonly List<T> _values = [];

    public EntityComponentBatchAccessor(World world)
    {
        _world = world;
        _componentTypeId = ComponentRegistry.GetComponentTypeId<T>();
    }

    /// <summary>
    /// Queue an entity to fetch component T from.
    /// </summary>
    public void QueueRead(Entity entity)
    {
        _entities.Add(entity);
    }

    /// <summary>
    /// Fetch all queued components in a single batch operation.
    /// Returns a list of component values in the same order as queued.
    /// </summary>
    public List<T> FetchBatch()
    {
        // This would be optimized to fetch all components in one native call
        // rather than individual calls per entity
        _values.Clear();
        
        // Placeholder: actual implementation would batch fetch from native ECS
        for (int i = 0; i < _entities.Count; i++)
        {
            _values.Add(default!);
        }

        return _values;
    }

    /// <summary>
    /// Write the same component value to all queued entities in one batch.
    /// </summary>
    public void WriteBatchSameValue(T value)
    {
        // This would be optimized to write all components in one native call
        // Placeholder implementation
    }

    /// <summary>
    /// Write different component values to queued entities in one batch.
    /// Values list must match queued entities count.
    /// </summary>
    public void WriteBatchValues(List<T> values)
    {
        if (values.Count != _entities.Count)
        {
            throw new ArgumentException(
                $"Values count ({values.Count}) must match entities count ({_entities.Count})"
            );
        }

        // This would be optimized to write all components in one native call
        // Placeholder implementation
    }

    /// <summary>
    /// Clear queued entities without fetching.
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
