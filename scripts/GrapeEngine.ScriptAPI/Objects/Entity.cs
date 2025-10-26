using GrapeEngine.ScriptAPI.Unsafe;
using GrapeEngine.Scripting;

namespace GrapeEngine.Scripting
{
    public class Entity
    {
        public ulong EntityId { get; private set; }

        public Entity(ulong entityId)
        {
            EntityId = entityId;
        }

        /// <summary>
        /// Get a component from this entity.
        /// Returns null if the component doesn't exist.
        /// </summary>
        public T GetComponent<T>() where T : unmanaged
        {
            unsafe
            {
                var size = sizeof(T);
                var buffer = stackalloc byte[size]; // this is type byte*
                // stackalloc is basically C's malloc or new() in C++ but ON THE STACK (hence STACKalloc)

                if (EntityAPI.GetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), buffer, size))
                    return *(T*)buffer; // dereference and cast to T*

                // Throw if component not found
                throw new NullReferenceException($"Entity {EntityId} does not have component of type {typeof(T).Name}");
            }
        }

        /// <summary>
        /// Add or update a component on this entity.
        /// </summary>
        public void SetComponent<T>(T component) where T : unmanaged
        {
            unsafe // needed for pointer operations as C# is safe by default!
            {
                EntityAPI.SetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), &component, sizeof(T));
            }
        }

        /// <summary>
        /// Check if this entity has a component.
        /// </summary>
        public bool HasComponent<T>() where T : unmanaged
        {
            // This does not need the unsafe context as no pointers are used
            // Check the method above. It uses pointers. (&component)
            return EntityAPI.HasComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
        }

        /// <summary>
        /// Remove a component from this entity.
        /// </summary>
        public void RemoveComponent<T>() where T : unmanaged
        {
            EntityAPI.RemoveComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
        }
    }
}