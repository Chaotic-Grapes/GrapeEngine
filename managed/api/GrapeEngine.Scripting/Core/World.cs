/* Start Header *****************************************************************/
/*!
\file   World.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for the C++ ECS World. Provides access to entities and components
from C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Query;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Represents the ECS World, which contains all entities and their components.
/// Use this to create, destroy, and query entities from C# systems.
/// </summary>
public class World
{
    private readonly IntPtr _nativeWorldPtr;

    /// <summary>
    /// Internal constructor. World instances are created by the engine.
    /// </summary>
    internal unsafe World(void* nativeWorldPtr)
    {
        _nativeWorldPtr = (IntPtr)nativeWorldPtr;
    }

    /// <summary>
    /// Internal accessor for native World pointer.
    /// </summary>
    internal unsafe void* NativePtr => (void*)_nativeWorldPtr;

    // ============================================================================
    // Entity Lifecycle
    // ============================================================================

    /// <summary>
    /// Create a new entity in the world.
    /// </summary>
    /// <returns>A new Entity instance</returns>
    public Entity CreateEntity()
    {
        unsafe
        {
            ulong entityId = WorldAPI.CreateEntity((void*)_nativeWorldPtr);
            return new Entity(this, entityId);
        }
    }

    /// <summary>
    /// Check if an entity is alive (valid).
    /// </summary>
    /// <param name="entity">The entity to check</param>
    /// <returns>True if the entity is alive, false otherwise</returns>
    public bool IsAlive(Entity entity)
    {
        unsafe
        {
            return WorldAPI.IsEntityAlive((void*)_nativeWorldPtr, entity.Id);
        }
    }

    /// <summary>
    /// Destroy an entity in the world.
    /// </summary>
    /// <param name="entity">The entity to destroy</param>
    public void DestroyEntity(Entity entity)
    {
        if (entity == null)
            throw new ArgumentNullException(nameof(entity));

        unsafe
        {
            WorldAPI.DestroyEntity((void*)_nativeWorldPtr, entity.Id);
        }
    }

    /// <summary>
    /// Instantiate an entity from an archetype.
    /// </summary>
    /// <param name="archetypeId">The archetype ID to instantiate from</param>
    /// <returns>A new Entity instance</returns>
    public Entity InstantiateEntity(uint archetypeId)
    {
        unsafe
        {
            ulong entityId = WorldAPI.CreateEntity((void*)_nativeWorldPtr);
            return new Entity(this, entityId);
        }
    }

    /// <summary>
    /// Clone an existing entity.
    /// </summary>
    /// <param name="sourceEntity">The entity to clone</param>
    /// <returns>A new cloned Entity instance</returns>
    public Entity CloneEntity(Entity sourceEntity)
    {
        if (sourceEntity == null)
            throw new ArgumentNullException(nameof(sourceEntity));

        unsafe
        {
            ulong entityId = WorldAPI.CreateEntity((void*)_nativeWorldPtr);
            return new Entity(this, entityId);
        }
    }

    // ============================================================================
    // Hierarchy Operations
    // ============================================================================

    /// <summary>
    /// Attach a child entity to a parent entity in the hierarchy.
    /// </summary>
    /// <param name="child">The child entity</param>
    /// <param name="parent">The parent entity</param>
    public void Attach(Entity child, Entity parent)
    {
        if (child == null)
            throw new ArgumentNullException(nameof(child));
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));

        unsafe
        {
            WorldAPI.AttachChild((void*)_nativeWorldPtr, child.Id, parent.Id);
        }
    }

    /// <summary>
    /// Detach a child entity from its parent.
    /// </summary>
    /// <param name="child">The child entity to detach</param>
    public void Detach(Entity child)
    {
        if (child == null)
            throw new ArgumentNullException(nameof(child));
        
        unsafe
        {
            WorldAPI.DetachChild((void*)_nativeWorldPtr, child.Id);
        }
    }

    /// <summary>
    /// Get the parent entity of a child entity.
    /// </summary>
    /// <param name="child">The child entity</param>
    /// <returns>The parent entity, or null if no parent</returns>
    public Entity? GetParent(Entity child)
    {
        if (child == null)
            throw new ArgumentNullException(nameof(child));

        unsafe
        {
            ulong parentId = WorldAPI.GetParent((void*)_nativeWorldPtr, child.Id);

            if (parentId == ulong.MaxValue)
                return null;

            var parent = new Entity(this, parentId);
            return IsAlive(parent) ? parent : null;
        }
    }

    /// <summary>
    /// Get the first child of a parent entity.
    /// </summary>
    /// <param name="parent">The parent entity</param>
    /// <returns>The first child entity, or null if no children</returns>
    public Entity? GetFirstChild(Entity parent)
    {
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));

        unsafe
        {
            ulong childId = WorldAPI.GetFirstChild((void*)_nativeWorldPtr, parent.Id);

            if (childId == ulong.MaxValue)
                return null;

            var child = new Entity(this, childId);
            return IsAlive(child) ? child : null;
        }
    }

    /// <summary>
    /// Get the next sibling of an entity.
    /// </summary>
    /// <param name="child">The child entity</param>
    /// <returns>The next sibling, or null if no more siblings</returns>
    public Entity? GetNextSibling(Entity child)
    {
        if (child == null)
            throw new ArgumentNullException(nameof(child));
        
        unsafe
        {
            ulong siblingId = WorldAPI.GetNextSibling((void*)_nativeWorldPtr, child.Id);

            if (siblingId == ulong.MaxValue)
                return null;

            var sibling = new Entity(this, siblingId);
            return IsAlive(sibling) ? sibling : null;
        }
    }

    /// <summary>
    /// Get the number of direct children a parent entity has.
    /// </summary>
    /// <param name="parent">The parent entity</param>
    /// <returns>The child count</returns>
    public int GetChildCount(Entity parent)
    {
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));
        
        unsafe
        {
            return WorldAPI.GetChildCount((void*)_nativeWorldPtr, parent.Id);
        }
    }

    /// <summary>
    /// Iterate over all direct children of a parent entity.
    /// </summary>
    /// <param name="parent">The parent entity</param>
    /// <param name="action">Action to invoke for each child</param>
    public void ForEachChild(Entity parent, Action<Entity> action)
    {
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));
        if (action == null)
            throw new ArgumentNullException(nameof(action));

        Entity? child = GetFirstChild(parent);
        while (child != null)
        {
            action(child);
            child = GetNextSibling(child);
        }
    }

    /// <summary>
    /// Get all direct children of a parent entity as a list.
    /// </summary>
    /// <param name="parent">The parent entity</param>
    /// <returns>List of child entities</returns>
    public List<Entity> GetChildren(Entity parent)
    {
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));

        var children = new List<Entity>(GetChildCount(parent));
        ForEachChild(parent, child => children.Add(child));
        return children;
    }

    // ============================================================================
    // Query Operations
    // ============================================================================

    /// <summary>
    /// Create a query for iterating over entities with one component type.
    /// </summary>
    public Query<T1> Query<T1>() where T1 : unmanaged
    {
        return new Query<T1>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with two component types.
    /// </summary>
    public Query<T1, T2> Query<T1, T2>()
        where T1 : unmanaged
        where T2 : unmanaged
    {
        return new Query<T1, T2>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with three component types.
    /// </summary>
    public Query<T1, T2, T3> Query<T1, T2, T3>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
    {
        return new Query<T1, T2, T3>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with four component types.
    /// </summary>
    public Query<T1, T2, T3, T4> Query<T1, T2, T3, T4>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
    {
        return new Query<T1, T2, T3, T4>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with five component types.
    /// </summary>
    public Query<T1, T2, T3, T4, T5> Query<T1, T2, T3, T4, T5>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
    {
        return new Query<T1, T2, T3, T4, T5>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with six component types.
    /// </summary>
    public Query<T1, T2, T3, T4, T5, T6> Query<T1, T2, T3, T4, T5, T6>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
        where T6 : unmanaged
    {
        return new Query<T1, T2, T3, T4, T5, T6>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with seven component types.
    /// </summary>
    public Query<T1, T2, T3, T4, T5, T6, T7> Query<T1, T2, T3, T4, T5, T6, T7>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
        where T6 : unmanaged
        where T7 : unmanaged
    {
        return new Query<T1, T2, T3, T4, T5, T6, T7>(this);
    }

    /// <summary>
    /// Create a query for iterating over entities with eight component types.
    /// </summary>
    public Query<T1, T2, T3, T4, T5, T6, T7, T8> Query<T1, T2, T3, T4, T5, T6, T7, T8>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
        where T6 : unmanaged
        where T7 : unmanaged
        where T8 : unmanaged
    {
        return new Query<T1, T2, T3, T4, T5, T6, T7, T8>(this);
    }

    // ============================================================================
    // Job System Access
    // ============================================================================

    private Job.JobManager? _jobManager;

    /// <summary>
    /// Access the job manager for this world.
    /// Use this to schedule jobs and manage parallelized work.
    /// </summary>
    public Job.JobManager JobManager
    {
        get
        {
            if (_jobManager == null)
            {
                unsafe
                {
                    nint jobManagerPtr = WorldAPI.GetJobManager((void*)_nativeWorldPtr);
                    _jobManager = new Job.JobManager(jobManagerPtr);
                }
            }
            return _jobManager;
        }
    }
}

