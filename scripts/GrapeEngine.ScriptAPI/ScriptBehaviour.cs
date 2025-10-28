using System;
using System.Runtime.InteropServices;
using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Base class for all entity scripts in GrapeEngine.
    /// Inherit from this class to create custom behaviors for entities.
    /// </summary>
    public abstract class ScriptBehaviour
    {
        private Entity? _entity;

        /// <summary>
        /// The unique identifier of the entity this script is attached to.
        /// </summary>
        public ulong EntityId { get; internal set; }

        /// <summary>
        /// The entity this script is attached to.
        /// Provides convenient access to component operations.
        /// </summary>
        public Entity Entity
        {
            get
            {
                if (_entity == null || _entity.EntityId != EntityId)
                    _entity = new Entity(EntityId);
                return _entity;
            }
        }

        // ============================================================================
        // Lifecycle Methods (Override these in derived classes)
        // ============================================================================

        /// <summary>
        /// Called once when the script is first initialized.
        /// Use this for setup and initialization logic.
        /// </summary>
        public virtual void OnStart() { }

        /// <summary>
        /// Called every frame while the entity is active.
        /// </summary>
        public virtual void OnUpdate() { }

        /// <summary>
        /// Called at fixed time intervals for physics updates.
        /// </summary>
        public virtual void OnFixedUpdate() { }

        /// <summary>
        /// Called every frame after all OnUpdate calls.
        /// </summary>
        public virtual void OnLateUpdate() { }

        /// <summary>
        /// Called when the script is enabled.
        /// </summary>
        public virtual void OnEnable() { }

        /// <summary>
        /// Called when the script is disabled.
        /// </summary>
        public virtual void OnDisable() { }

        /// <summary>
        /// Called when the script or entity is being destroyed.
        /// Use this for cleanup logic.
        /// </summary>
        public virtual void OnDestroy() { }

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
        protected T GetComponent<T>() where T : unmanaged
        {
            return Entity.GetComponent<T>();
        }

        /// <summary>
        /// Add a component to this entity and return it.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <param name="component">The component data to add</param>
        /// <returns>The added component, if successful</returns>
        protected T AddComponent<T>(T component) where T : unmanaged
        {
            return Entity.AddComponent(component);
        }

        /// <summary>
        /// Check if this entity has the specified component.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <returns>True if entity has the component, false otherwise</returns>
        protected bool HasComponent<T>() where T : unmanaged
        {
            return Entity.HasComponent<T>();
        }

        /// <summary>
        /// Try to get a component from this entity.
        /// Returns false if the component doesn't exist.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <param name="component">The component data if found</param>
        /// <returns>True if the component exists, false otherwise</returns>
        protected bool TryGetComponent<T>(out T component) where T : unmanaged
        {
            return Entity.TryGetComponent(out component);
        }

        /// <summary>
        /// Set (add or update) a component on this entity.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <param name="component">The component data to set</param>
        protected void SetComponent<T>(T component) where T : unmanaged
        {
            Entity.SetComponent(component);
        }

        /// <summary>
        /// Remove a component from this entity.
        /// Does nothing if the component doesn't exist.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        protected void RemoveComponent<T>() where T : unmanaged
        {
            Entity.RemoveComponent<T>();
        }

        // ============================================================================
        // Entity Management
        // ============================================================================

        /// <summary>
        /// Destroy this entity.
        /// </summary>
        protected void DestroyEntity()
        {
            Entity.Destroy();
        }

        /// <summary>
        /// Destroy another entity by its ID.
        /// </summary>
        /// <param name="entityId">The entity ID to destroy</param>
        protected void DestroyEntity(ulong entityId)
        {
            EntityAPI.DestroyEntity(entityId);
        }

        /// <summary>
        /// Destroy another entity.
        /// </summary>
        /// <param name="entity">The entity to destroy</param>
        protected void DestroyEntity(Entity entity)
        {
            entity.Destroy();
        }

        /// <summary>
        /// Create a new entity in the world.
        /// </summary>
        /// <returns>The newly created entity</returns>
        protected Entity CreateEntity()
        {
            ulong entityId = EntityAPI.CreateEntity();
            return new Entity(entityId);
        }

        // Note: params T[] will not work as this means all the components are of the same type.
        // Note2: object[] is not ideal for performance and ECS is really concerned with performance.
        // Therefore, see IComponentData and ComponentData<T>!
        // IComponentData is a wrapper interface to add components of different types.
        /// <summary>
        /// Create a new entity in the world with the specified components.
        /// </summary>
        /// <param name="components">The components to add to the new entity</param>
        /// <returns>The newly created entity</returns>
        protected Entity CreateEntity(params IComponentData[] components)
        {
            ulong entityId = EntityAPI.CreateEntity();
            Entity entity = new(entityId); // Instantiate Entity with the new ID

            foreach (var component in components)
                // Unsafe not needed
                component.AddToEntity(entity);

            return entity;
            // IComponentData array is cleaned up by GC automatically
        }

        // ============================================================================
        // Utility Methods
        // ============================================================================

        /// <summary>
        /// Log a message to console.
        /// </summary>
        /// <param name="message">The message to log</param>
        /// <param name="level">The severity of the log</param>
        protected void Log(string message, LogLevel level = LogLevel.Info)
            => Logging.Log(message, level);
    }
}
