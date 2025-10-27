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
        // Component Access (Convenience methods - delegate to Entity)
        // ============================================================================

        /// <summary>
        /// Get a component from this entity.
        /// Throws an exception if the component doesn't exist.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <returns>The component data</returns>
        protected T GetComponent<T>() where T : unmanaged
        {
            return Entity.GetComponent<T>();
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
        /// Check if this entity has a component.
        /// </summary>
        /// <typeparam name="T">The component type</typeparam>
        /// <returns>True if the entity has the component, false otherwise</returns>
        protected bool HasComponent<T>() where T : unmanaged
        {
            return Entity.HasComponent<T>();
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
        /// After calling this, the entity will no longer be valid.
        /// </summary>
        protected void Destroy()
        {
            Entity.Destroy();
        }

        /// <summary>
        /// Destroy another entity by its ID.
        /// </summary>
        /// <param name="entityId">The entity ID to destroy</param>
        protected void Destroy(ulong entityId)
        {
            EntityAPI.DestroyEntity(entityId);
        }

        /// <summary>
        /// Destroy another entity.
        /// </summary>
        /// <param name="entity">The entity to destroy</param>
        protected void Destroy(Entity entity)
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
