using System;
using System.Collections.Generic;
using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Static World API for entity operations that don't belong to a specific entity.
    /// This provides a facade over the C++ World class.
    /// 
    /// Note: Many of these features require additional C++ API exports.
    /// </summary>
    public static class World
    {
        // ============================================================================
        // Entity Creation/Destruction (Requires C++ API exports)
        // ============================================================================

        /// <summary>
        /// Create a new entity.
        /// </summary>
        /// <returns>The newly created entity</returns>
        public static Entity CreateEntity()
        {
            // TODO: Implement when CreateEntity API is exposed from C++
            // ulong entityId = EntityAPI.CreateEntity();
            // return new Entity(entityId);
            throw new NotImplementedException("CreateEntity API not yet exposed from C++.");
        }

        /// <summary>
        /// Destroy an entity.
        /// </summary>
        /// <param name="entity">The entity to destroy</param>
        public static void DestroyEntity(Entity entity)
        {
            EntityAPI.DestroyEntity(entity.EntityId);
        }

        /// <summary>
        /// Destroy an entity by ID.
        /// </summary>
        /// <param name="entityId">The entity ID to destroy</param>
        public static void DestroyEntity(ulong entityId)
        {
            EntityAPI.DestroyEntity(entityId);
        }

        // ============================================================================
        // Entity Queries (Requires C++ API exports)
        // ============================================================================

        /// <summary>
        /// Find all entities that have a specific component.
        /// </summary>
        /// <typeparam name="T">The component type to query for</typeparam>
        /// <returns>List of entities with the component</returns>
        public static List<Entity> FindEntitiesWithComponent<T>() where T : unmanaged
        {
            // TODO: Implement when QueryEntities API is exposed from C++
            // This would require:
            // 1. C++ function that takes a component type hash
            // 2. Returns an array of entity IDs
            // 3. Marshal the array to C#
            throw new NotImplementedException("FindEntitiesWithComponent API not yet exposed from C++");
        }

        /// <summary>
        /// Find the first entity that has a specific component.
        /// </summary>
        /// <typeparam name="T">The component type to query for</typeparam>
        /// <returns>The first entity with the component, or null if not found</returns>
        public static Entity? FindFirstEntityWithComponent<T>() where T : unmanaged
        {
            // TODO: Implement when QueryEntities API is exposed from C++
            throw new NotImplementedException("FindFirstEntityWithComponent API not yet exposed from C++");
        }

        /// <summary>
        /// Find entities by tag mask.
        /// </summary>
        /// <param name="tagMask">The tag mask to search for</param>
        /// <returns>List of entities with matching tags</returns>
        public static List<Entity> FindEntitiesByTag(uint tagMask)
        {
            // TODO: Implement when QueryEntities API is exposed from C++
            throw new NotImplementedException("FindEntitiesByTag API not yet exposed from C++");
        }

        /// <summary>
        /// Find entities by name.
        /// </summary>
        /// <param name="name">The name to search for</param>
        /// <returns>List of entities with the given name</returns>
        public static List<Entity> FindEntitiesByName(string name)
        {
            // TODO: Implement when name-based query API is exposed from C++
            throw new NotImplementedException("FindEntitiesByName API not yet exposed from C++");
        }

        // ============================================================================
        // World Queries (Statistics and Info)
        // ============================================================================

        /// <summary>
        /// Get the total number of active entities.
        /// </summary>
        /// <returns>The number of active entities</returns>
        public static int GetEntityCount()
        {
            // TODO: Implement when GetEntityCount API is exposed from C++
            throw new NotImplementedException("GetEntityCount API not yet exposed from C++");
        }

        /// <summary>
        /// Get the number of entities with a specific component.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <returns>The count of entities with the component</returns>
        public static int GetComponentCount<T>() where T : unmanaged
        {
            // TODO: Implement when component count API is exposed from C++
            throw new NotImplementedException("GetComponentCount API not yet exposed from C++");
        }

        // ============================================================================
        // Utility Methods
        // ============================================================================

        /// <summary>
        /// Check if an entity ID is valid (not null/zero).
        /// </summary>
        /// <param name="entityId">The entity ID to check</param>
        /// <returns>True if valid, false if null/zero</returns>
        public static bool IsValidEntityId(ulong entityId)
        {
            return entityId != 0;
        }

        /// <summary>
        /// Create an Entity wrapper from a packed entity ID.
        /// </summary>
        /// <param name="entityId">The packed entity ID</param>
        /// <returns>An Entity instance</returns>
        public static Entity GetEntity(ulong entityId)
        {
            return new Entity(entityId);
        }
    }
}
