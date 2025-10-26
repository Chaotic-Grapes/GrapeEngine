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
        /// <summary>
        /// The unique identifier of the entity this script is attached to.
        /// </summary>
        public ulong EntityId { get; internal set; }

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

        /// <summary>
        /// Destroy an entity.
        /// </summary>
        public void Destroy(ulong entityId)
        {
            EntityAPI.DestroyEntity(entityId);
        }

        /// <summary>
        /// Log a message to console.
        /// </summary>
        /// <param name="message">The message to log</param>
        /// <param name="level">The severity of the log</param>
        protected void Log(string message, LogLevel level = LogLevel.Info)
            => Logging.Log(message, level);
    }
}
