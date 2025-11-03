using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine.Scripting;

/// <summary>
/// Represents an entity in the ECS system.
/// </summary>
/// <param name="entityId">The packed entity ID</param>
public class Entity(ulong entityId)
{
    /// <summary>
    /// The packed entity ID.
    /// </summary>
    public ulong EntityId { get; private set; } = entityId;

    // ============================================================================
    // Component Access
    // ============================================================================

    /// <summary>
    /// Get a component from this entity.
    /// Throws an exception if the component doesn't exist.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    /// <returns>The component data or null</returns>
    //public T GetComponent<T>() where T : unmanaged
    //{
    //    unsafe // unsafe context needed for pointer operations as C# is safe by default
    //    {
    //        var size = sizeof(T);
    //        var buffer = stackalloc byte[size]; // this is type byte*
    //        // stackalloc is basically C's malloc or new() in C++ but ON THE STACK (hence STACKalloc)

    //        if (EntityAPI.GetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), buffer, size))
    //            return *(T*)buffer; // dereference pointer and cast to T*

    //        return default;
    //    }
    //}

    /// <summary>
    /// Get a mutable component from this entity.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    /// <returns>The component T or a <see cref="System.Runtime.CompilerServices.Unsafe.NullRef{T}"/> when not found</returns>
    /// <remarks>
    ///     Check if the entity has the component with <see cref="Entity.HasComponent"/> or<br/>
    ///     use <see cref="Entity.TryGetComponent{T}(out bool)"/>.
    /// </remarks>
    public ref T GetComponent<T>() where T : unmanaged
    {
        unsafe
        {
            var ptr = EntityAPI.GetComponentPtr(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
            if (ptr == null)
                return ref System.Runtime.CompilerServices.Unsafe.NullRef<T>(); // Return a null-ref if component not found

            return ref *(T*)ptr;
        }
    }

    /// <summary>
    /// Add a component to this entity and return it.
    /// Throws an exception if adding the component fails.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    /// <param name="component">The component data to add</param>
    /// <returns>The added component or null</returns>
    public T AddComponent<T>(T component) where T : unmanaged
    {
        unsafe
        {
            var size = sizeof(T);
            var buffer = stackalloc byte[size]; // allocate buffer for the result

            if (EntityAPI.AddComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), &component, sizeof(T), buffer))
                return *(T*)buffer;

            return default;
        }
    }

    /// <summary>
    /// Check if this entity has the specified component.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    /// <returns>True if entity has the component, false otherwise</returns>
    public bool HasComponent<T>() where T : unmanaged
    {
        // This does not need the unsafe context as no pointers are used
        // Check the other methods above. It uses pointers. (&component)
        return EntityAPI.HasComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
    }

    /// <summary>
    /// Try to get a mutable component from this entity.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    /// <param name="hasComponent">True if component exists, false otherwise</param>
    /// <returns>The component T or a <see cref="System.Runtime.CompilerServices.Unsafe.NullRef{T}"/> when not found</returns>
    public ref T TryGetComponent<T>(out bool hasComponent) where T : unmanaged
    {
        unsafe
        {
            var ptr = EntityAPI.GetComponentPtr(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
            if (ptr != null)
            {
                hasComponent = true;
                return ref *(T*)ptr;
            }

            hasComponent = false;
            return ref System.Runtime.CompilerServices.Unsafe.NullRef<T>();
        }
    }

    /// <summary>
    /// Add or update a component on this entity.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    /// <param name="component">The component data to set</param>
    public void SetComponent<T>(T component) where T : unmanaged
    {
        unsafe
        {
            EntityAPI.SetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), &component, sizeof(T));
        }
    }

    /// <summary>
    /// Remove a component from this entity.
    /// </summary>
    /// <typeparam name="T">The component type</typeparam>
    public void RemoveComponent<T>() where T : unmanaged
    {
        // Might throw an exception if the component doesn't exist on C++ side I believe
        EntityAPI.RemoveComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
    }

    /// <summary>
    /// Destroy the entity.
    /// </summary>
    public void Destroy()
        => EntityAPI.DestroyEntity(EntityId);

    /// <summary>
    /// Check if this entity is not destroyed.
    /// </summary>
    /// <returns>True if entity is still alive, false otherwise</returns>
    public bool IsAlive()
        => EntityAPI.IsAlive(EntityId);

    // ============================================================================
    // Utility Methods
    // ============================================================================


    // TODO: Implement when IsAlive API is exposed from C++
    // public bool IsAlive()
    // {
    // }

    // ============================================================================
    // Operators and Equality
    // ============================================================================

    /// <summary>
    /// Equality comparison based on entity ID.
    /// </summary>
    public override bool Equals(object? obj)
    {
        if (obj is Entity other)
            return EntityId == other.EntityId;
        return false;
    }

    /// <summary>
    /// Hash code based on entity ID.
    /// </summary>
    public override int GetHashCode()
    {
        return EntityId.GetHashCode();
    }

    /// <summary>
    /// String representation of the entity.
    /// </summary>
    public override string ToString()
    {
        // Return just the packed entity ID as string
        return EntityId.ToString();
    }

    /// <summary>
    /// Equality operator.
    /// </summary>
    public static bool operator ==(Entity? left, Entity? right)
    {
        if (ReferenceEquals(left, right))
            return true;
        if (left is null || right is null)
            return false;
        return left.EntityId == right.EntityId;
    }

    /// <summary>
    /// Inequality operator.
    /// </summary>
    public static bool operator !=(Entity? left, Entity? right)
    {
        return !(left == right);
    }

    // ============================================================================
    // Static Utility
    // ============================================================================

    /// <summary>
    /// Create an Entity wrapper from a packed entity ID.
    /// </summary>
    /// <param name="entityId">The packed entity ID</param>
    /// <returns>An Entity instance</returns>
    public static Entity FromId(ulong entityId)
        => new(entityId); // Honestly this is not needed since you can just use the ctor

    /// <summary>
    /// Invalid/null entity constant.
    /// </summary>
    public static readonly Entity Invalid = new(0);
}