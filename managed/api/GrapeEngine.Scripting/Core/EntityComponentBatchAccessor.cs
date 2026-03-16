using System.Runtime.InteropServices;
using UnsafePtr = System.Runtime.CompilerServices.Unsafe;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Core;

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
                        ref var existing = ref entity.GetComponent<T>();
                        existing = value;
                    }
                    else
                    {
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
                        ref var existing = ref entity.GetComponent<T>();
                        existing = value;
                    }
                    else
                    {
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
