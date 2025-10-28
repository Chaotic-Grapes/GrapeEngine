using System;
using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Represents an entity in the ECS system.
    /// Provides access to component operations for any entity, not just the one a script is attached to.
    /// </summary>
    public class Entity
    {
        /// <summary>
        /// The packed entity ID.
        /// </summary>
        public ulong EntityId { get; private set; }

        /// <summary>
        /// Creates a new Entity wrapper around an entity ID.
        /// </summary>
        /// <param name="entityId">The packed entity ID</param>
        public Entity(ulong entityId)
        {
            EntityId = entityId;
        }

        // ============================================================================
        // Component Access
        // ============================================================================

        /// <summary>
        /// Get a component from this entity.
        /// Throws an exception if the component doesn't exist.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <returns>The component data</returns>
        /// <exception cref="InvalidOperationException">Thrown when the entity doesn't have the component</exception>
        public T GetComponent<T>() where T : unmanaged
        {
            unsafe // unsafe context needed for pointer operations as C# is safe by default
            {
                var size = sizeof(T);
                var buffer = stackalloc byte[size]; // this is type byte*
                // stackalloc is basically C's malloc or new() in C++ but ON THE STACK (hence STACKalloc)

                if (EntityAPI.GetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), buffer, size))
                    return *(T*)buffer; // dereference pointer and cast to T*

                throw new InvalidOperationException($"Entity {EntityId} does not have component of type {typeof(T).Name}");
            }
        }

        /// <summary>
        /// Add a component to this entity and return it.
        /// Throws an exception if adding the component fails.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <param name="component">The component data to add</param>
        /// <returns>The added component, if successful</returns>
        /// <exception cref="InvalidOperationException">Thrown when adding the component fails</exception>
        public T AddComponent<T>(T component) where T : unmanaged
        {
            unsafe
            {
                var size = sizeof(T);
                var buffer = stackalloc byte[size]; // allocate buffer for the result

                if (EntityAPI.AddComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), &component, sizeof(T), buffer))
                    return *(T*)buffer;
                
                throw new InvalidOperationException($"Failed to add component of type {typeof(T).Name} to entity {EntityId}");
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
        /// Try to get a component from this entity.
        /// Returns false if the component doesn't exist.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <param name="component">The component data if found</param>
        /// <returns>True if the component exists, false otherwise</returns>
        public bool TryGetComponent<T>(out T component) where T : unmanaged
        {
            unsafe
            {
                var size = sizeof(T);
                var buffer = stackalloc byte[size];

                // Try to get the component data
                if (EntityAPI.GetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), buffer, size))
                {
                    // Component exists, marshal data
                    component = *(T*)buffer;
                    return true;
                }

                // Component does not exist, assign default and return false
                component = default; // out param must be assigned, so assign default
                return false;
            }
        }

        /// <summary>
        /// Set (add or update) a component on this entity.
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
        /// Destroy this entity.
        /// After calling this, the entity will no longer be valid.
        /// </summary>
        public void Destroy()
        {
            EntityAPI.DestroyEntity(EntityId);
        }

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
            return $"Entity({EntityId})";
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
        {
            return new Entity(entityId);
        }

        /// <summary>
        /// Invalid/null entity constant.
        /// </summary>
        public static readonly Entity Invalid = new(0);
    }
}