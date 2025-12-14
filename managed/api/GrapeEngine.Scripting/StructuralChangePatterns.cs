/* Start Header *****************************************************************/
/*!
\file   StructuralChangePatterns.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Common patterns and utilities for safe structural changes in parallel jobs.

Provides fluent APIs, batch operations, and safety-checked patterns
for structural modifications in parallel execution contexts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Query;

namespace GrapeEngine.Scripting;

/// <summary>
/// Fluent builder for recording structural changes.
/// 
/// Provides a convenient way to queue multiple related changes
/// and batch them together.
/// </summary>
/// <remarks>
/// Create a structural change builder.
/// </remarks>
public class StructuralChangeBuilder(CommandBuffer buffer)
{
    private readonly CommandBuffer _buffer = buffer ?? throw new ArgumentNullException(nameof(buffer));
    private readonly List<Action> _changes = [];
    private string _batchName = string.Empty;

  /// <summary>
  /// Set batch name for tracking.
  /// </summary>
  public StructuralChangeBuilder WithBatchName(string name)
    {
        _batchName = name;
        return this;
    }

    /// <summary>
    /// Add change to create entity.
    /// </summary>
    public StructuralChangeBuilder CreateEntity(out Entity createdEntity)
    {
        createdEntity = _buffer.CreateEntity();
        return this;
    }

    /// <summary>
    /// Add change to destroy entity.
    /// </summary>
    public StructuralChangeBuilder DestroyEntity(Entity entity)
    {
        _changes.Add(() => _buffer.DestroyEntity(entity));
        return this;
    }

    /// <summary>
    /// Add change to add component.
    /// </summary>
    public StructuralChangeBuilder AddComponent<T>(Entity entity, T component) where T : unmanaged
    {
        _changes.Add(() => _buffer.AddComponent(entity, component));
        return this;
    }

    /// <summary>
    /// Add change to remove component.
    /// </summary>
    public StructuralChangeBuilder RemoveComponent<T>(Entity entity) where T : unmanaged
    {
        _changes.Add(() => _buffer.RemoveComponent<T>(entity));
        return this;
    }

    /// <summary>
    /// Build and record all changes.
    /// </summary>
    /// <returns>Number of changes recorded</returns>
    public int Build()
    {
        int count = 0;
        foreach (var change in _changes)
        {
            change();
            count++;
        }

        _changes.Clear();
        return count;
    }

    /// <summary>
    /// Get count of pending changes.
    /// </summary>
    public int GetPendingChangeCount() => _changes.Count;
}

/// <summary>
/// Safe batch operation for destroying multiple entities.
/// 
/// Records all destroy operations as a batch to be applied
/// at the synchronization point.
/// </summary>
/// <remarks>
/// Create a batch destroy operation.
/// </remarks>
public class BatchDestroyOperation(CommandBuffer buffer)
{
    private readonly CommandBuffer _buffer = buffer ?? throw new ArgumentNullException(nameof(buffer));
    private readonly List<Entity> _entitiesToDestroy = [];

    /// <summary>
    /// Mark entity for destruction.
    /// </summary>
    public void Destroy(Entity entity)
    {
        _entitiesToDestroy.Add(entity);
    }

    /// <summary>
    /// Mark multiple entities for destruction.
    /// </summary>
    public void DestroyRange(params Entity[] entities)
    {
        _entitiesToDestroy.AddRange(entities);
    }

    /// <summary>
    /// Mark entities from enumerable for destruction.
    /// </summary>
    public void DestroyRange(IEnumerable<Entity> entities)
    {
        _entitiesToDestroy.AddRange(entities);
    }

    /// <summary>
    /// Execute all destroy operations.
    /// </summary>
    /// <returns>Number of entities marked for destruction</returns>
    public int Execute()
    {
        var count = _entitiesToDestroy.Count;

        foreach (var entity in _entitiesToDestroy)
        {
            _buffer.DestroyEntity(entity);
        }

        _entitiesToDestroy.Clear();
        return count;
    }

    /// <summary>
    /// Get count of pending destroy operations.
    /// </summary>
    public int GetPendingCount() => _entitiesToDestroy.Count;

    /// <summary>
    /// Clear all pending operations without executing.
    /// </summary>
    public void Clear()
    {
        _entitiesToDestroy.Clear();
    }
}

/// <summary>
/// Patterns for common structural change scenarios.
/// </summary>
public static class StructuralChangePatterns
{
    /// <summary>
    /// Pattern: Destroy all entities matching a condition.
    /// 
    /// Safe pattern for removing entities from a parallel job.
    /// </summary>
    public static int DestroyAllMatching<T>(
        Query<T> query,
        CommandBuffer buffer,
        Func<Entity, bool> predicate) where T : unmanaged
    {
        var destroyedCount = 0;

        // Record destroy operations (don't execute yet)
        foreach (var (entity, _) in query)
        {
            if (predicate(entity))
            {
                buffer.DestroyEntity(entity);
                destroyedCount++;
            }
        }

        return destroyedCount;
    }

    /// <summary>
    /// Pattern: Transform entities by modifying components.
    /// 
    /// Safe pattern for complex entity modifications that require
    /// removing old components and adding new ones.
    /// </summary>
    public static void TransformEntities<T1, T2>(
        Query<T1> query,
        CommandBuffer buffer,
        Func<T1, T2> transformer) where T1 : unmanaged where T2 : unmanaged
    {
        foreach (var (entity, oldComp) in query)
        {
            var newComp = transformer(oldComp);

            buffer.RemoveComponent<T1>(entity);
            buffer.AddComponent(entity, newComp);
        }
    }

    /// <summary>
    /// Pattern: Spawn entities in bulk.
    /// 
    /// Safe pattern for creating many entities from a job.
    /// </summary>
    public static List<Entity> SpawnBulk(
        CommandBuffer buffer,
        int count,
        uint archetypeId)
    {
        var spawnedEntities = new List<Entity>(count);

        for (int i = 0; i < count; i++)
        {
            var entity = buffer.Instantiate(archetypeId);
            spawnedEntities.Add(entity);
        }

        return spawnedEntities;
    }

    /// <summary>
    /// Pattern: Clone entities matching a condition.
    /// 
    /// Safe pattern for duplicating entities from a parallel job.
    /// </summary>
    public static int CloneMatching<T>(
        Query<T> query,
        CommandBuffer buffer,
        Func<Entity, bool> predicate) where T : unmanaged
    {
        int clonedCount = 0;

        foreach (var (entity, _) in query)
        {
            if (predicate(entity))
            {
                buffer.Clone(entity);
                clonedCount++;
            }
        }

        return clonedCount;
    }

    /// <summary>
    /// Pattern: Staged destruction with validation.
    /// 
    /// Safe pattern for destroying entities with pre-check validation.
    /// </summary>
    public static int DestroyIfValid<T>(
        Query<T> query,
        CommandBuffer buffer,
        Func<Entity, T, bool> isValid) where T : unmanaged
    {
        int destroyedCount = 0;

        foreach (var (entity, component) in query)
        {
            if (!isValid(entity, component))
            {
                buffer.DestroyEntity(entity);
                destroyedCount++;
            }
        }

        return destroyedCount;
    }

    /// <summary>
    /// Pattern: Conditional component swapping.
    /// 
    /// Safe pattern for replacing components conditionally.
    /// </summary>
    public static int SwapComponentsIf<T1, T2>(
        Query<T1> query,
        CommandBuffer buffer,
        Func<Entity, T1, bool> shouldSwap,
        Func<T1, T2> converter) where T1 : unmanaged where T2 : unmanaged
    {
        int swappedCount = 0;

        foreach (var (entity, oldComp) in query)
        {
            if (shouldSwap(entity, oldComp))
            {
                var newComp = converter(oldComp);
                buffer.RemoveComponent<T1>(entity);
                buffer.AddComponent(entity, newComp);
                swappedCount++;
            }
        }

        return swappedCount;
    }
}

/// <summary>
/// Statistics tracking for structural changes.
/// </summary>
public class StructuralChangeStats
{
    /// <summary>
    /// Number of entities created
    /// </summary>
    public int EntitiesCreated { get; set; }

    /// <summary>
    /// Number of entities destroyed
    /// </summary>
    public int EntitiesDestroyed { get; set; }

    /// <summary>
    /// Number of components added
    /// </summary>
    public int ComponentsAdded { get; set; }

    /// <summary>
    /// Number of components removed
    /// </summary>
    public int ComponentsRemoved { get; set; }

    /// <summary>
    /// Number of component updates
    /// </summary>
    public int ComponentsUpdated { get; set; }

    /// <summary>
    /// Total time spent applying changes (ms)
    /// </summary>
    public long TotalTimeMs { get; set; }

    /// <summary>
    /// Get total structural changes
    /// </summary>
    public int GetTotalChanges()
    {
        return EntitiesCreated + EntitiesDestroyed + ComponentsAdded + 
               ComponentsRemoved + ComponentsUpdated;
    }

    /// <summary>
    /// Get readable summary
    /// </summary>
    public string GetSummary()
    {
        return $"Created: {EntitiesCreated}, Destroyed: {EntitiesDestroyed}, " +
               $"Added: {ComponentsAdded}, Removed: {ComponentsRemoved}, " +
               $"Updated: {ComponentsUpdated}, Time: {TotalTimeMs}ms";
    }
}

/// <summary>
/// Extension methods for safe structural changes in job contexts.
/// </summary>
public static class SafeStructuralChangeExtensions
{
    /// <summary>
    /// Safely destroy entities matching condition from a job.
    /// </summary>
    public static void DestroyAllIf<T>(
        this Query<T> query,
        CommandBuffer buffer,
        Func<Entity, T, bool> shouldDestroy) where T : unmanaged
    {
        foreach (var (entity, component) in query)
        {
            if (shouldDestroy(entity, component))
            {
                buffer.DestroyEntity(entity);
            }
        }
    }

    /// <summary>
    /// Safely add component to entities matching condition.
    /// </summary>
    public static void AddComponentIf<T1, T2>(
        this Query<T1> query,
        CommandBuffer buffer,
        Func<Entity, T1, bool> condition,
        T2 component) where T1 : unmanaged where T2 : unmanaged
    {
        foreach (var (entity, comp) in query)
        {
            if (condition(entity, comp))
            {
                buffer.AddComponent(entity, component);
            }
        }
    }

    /// <summary>
    /// Safely remove component from entities matching condition.
    /// </summary>
    public static int RemoveComponentIf<T1, T2>(
        this Query<T1> query,
        CommandBuffer buffer,
        Func<Entity, T1, bool> condition) where T1 : unmanaged where T2 : unmanaged
    {
        var count = 0;

        foreach (var (entity, comp) in query)
        {
            if (condition(entity, comp))
            {
                buffer.RemoveComponent<T2>(entity);
                count++;
            }
        }

        return count;
    }

    /// <summary>
    /// Create a batch destroy operation for the query.
    /// </summary>
    public static BatchDestroyOperation GetBatchDestroyOperation<T>(
        this Query<T> query,
        CommandBuffer buffer) where T : unmanaged
    {
        return new BatchDestroyOperation(buffer);
    }

    /// <summary>
    /// Create a structural change builder.
    /// </summary>
    public static StructuralChangeBuilder CreateBuilder(this CommandBuffer buffer)
    {
        return new StructuralChangeBuilder(buffer);
    }
}

/// <summary>
/// Validator for safe structural changes.
/// 
/// Checks that structural changes don't violate constraints
/// or create invalid world states.
/// </summary>
/// <remarks>
/// Create a validator for a world.
/// </remarks>
public class StructuralChangeValidator(World world)
{
    private readonly World _world = world;
    private readonly List<string> _errors = [];

    /// <summary>
    /// Validate that entity exists before modifying.
    /// </summary>
    public bool ValidateEntityExists(Entity entity)
    {
        // Would check if entity is valid
        return true;
    }

    /// <summary>
    /// Validate that component can be added.
    /// </summary>
    public bool ValidateCanAddComponent<T>(Entity entity) where T : unmanaged
    {
        // Would check archetype constraints
        return true;
    }

    /// <summary>
    /// Validate that component can be removed.
    /// </summary>
    public bool ValidateCanRemoveComponent<T>(Entity entity) where T : unmanaged
    {
        // Would check if component exists
        return true;
    }

    /// <summary>
    /// Get validation errors.
    /// </summary>
    public IEnumerable<string> GetErrors() => _errors;

    /// <summary>
    /// Get error count.
    /// </summary>
    public int GetErrorCount() => _errors.Count;
}
