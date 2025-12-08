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
    public unsafe Entity CreateEntity()
    {
        ulong entityId = WorldAPI.CreateEntity((void*)_nativeWorldPtr);
        return new Entity(this, entityId);
    }

    /// <summary>
    /// Check if an entity is alive (valid).
    /// </summary>
    /// <param name="entity">The entity to check</param>
    /// <returns>True if the entity is alive, false otherwise</returns>
    public unsafe bool IsAlive(Entity entity)
    {
        return WorldAPI.IsEntityAlive((void*)_nativeWorldPtr, entity.Id);
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
}
