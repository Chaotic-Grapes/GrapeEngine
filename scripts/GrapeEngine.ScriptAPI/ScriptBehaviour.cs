/* Start Header *****************************************************************/
/*!
\file   ScriptBehaviour.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th October 2025
\brief
Base class for all entity scripts in GrapeEngine. Inherit from this class to
create custom behaviors for entities.

\details
Provides lifecycle methods (OnStart, OnUpdate, etc.) and convenient access to
entity components and management functions.

\code
public class MyScript : ScriptBehaviour
{
    public override void OnStart()
    {
        Log("MyScript started!");
    }

    public override void OnUpdate()
    {
        // Custom update logic here
    }
}
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine.Scripting;

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
    // Collision Callbacks (Override these in game scripts)
    // ============================================================================

    /// <summary>
    /// Called when this entity starts colliding with another entity
    /// </summary>
    internal virtual void OnCollisionEnter(ulong otherEntity) { }

    /// <summary>
    /// Called every frame while colliding with another entity
    /// </summary>
    internal virtual void OnCollisionStay(ulong otherEntity) { }

    /// <summary>
    /// Called when this entity stops colliding with another entity
    /// </summary>
    internal virtual void OnCollisionExit(ulong otherEntity) { }

    // ============================================================================
    // Component Access
    // ============================================================================

    /// <inheritdoc cref="Entity.GetComponent{T}"/>
    protected ref T GetComponent<T>() where T : unmanaged
        => ref Entity.GetComponent<T>();

    /// <inheritdoc cref="Entity.AddComponent{T}(T)"/>
    protected T AddComponent<T>(T component) where T : unmanaged
        => Entity.AddComponent(component);

    /// <inheritdoc cref="Entity.HasComponent{T}"/>
    protected bool HasComponent<T>() where T : unmanaged
        => Entity.HasComponent<T>();

    /// <inheritdoc cref="Entity.TryGetComponent{T}(out T)"/>
    protected ref T TryGetComponent<T>(out bool hasComponent) where T : unmanaged
        => ref Entity.TryGetComponent<T>(out hasComponent);

    /// <inheritdoc cref="Entity.SetComponent{T}(T)"/>
    protected void SetComponent<T>(T component) where T : unmanaged
        => Entity.SetComponent(component);

    /// <inheritdoc cref="Entity.RemoveComponent{T}"/>
    protected void RemoveComponent<T>() where T : unmanaged
        => Entity.RemoveComponent<T>();

    // ============================================================================
    // Entity Management
    // ============================================================================

    /// <inheritdoc cref="Entity.Destroy"/>
    protected void DestroyEntity()
        => Entity.Destroy();

    /// <inheritdoc cref="Entity.Destroy"/>
    /// <param name="entityId">The packed entity ID</param>
    protected void DestroyEntity(ulong entityId)
        => EntityAPI.DestroyEntity(entityId);

    /// <inheritdoc cref="Entity.Destroy"/>
    /// <param name="entity">The entity to destroy</param>
    protected void DestroyEntity(Entity entity)
        => entity.Destroy();

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

    /// <inheritdoc cref="Entity.IsAlive"/>
    protected bool IsAlive()
        => Entity.IsAlive();

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
