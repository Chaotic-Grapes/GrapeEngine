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


namespace GrapeEngine;

/// <summary>
/// Represents the ECS World, which contains all entities and their components.
/// Use this to create, destroy, and query entities from C# systems.
/// </summary>
public unsafe class World
{
    private readonly void* _nativeWorldPtr;

    /// <summary>
    /// Internal constructor. World instances are created by the engine.
    /// </summary>
    internal World(void* nativeWorldPtr)
    {
        _nativeWorldPtr = nativeWorldPtr;
    }

    /// <summary>
    /// Internal accessor for native World pointer.
    /// </summary>
    internal void* NativePtr => _nativeWorldPtr;

    // ============================================================================
    // Entity Lifecycle
    // ============================================================================

    /// <summary>
    /// Create a new entity in the world.
    /// </summary>
    /// <returns>A new Entity instance</returns>
    public Entity CreateEntity()
    {
        ulong entityId = WorldInteropAPI.CreateEntity(_nativeWorldPtr);
        return new Entity(this, entityId);
    }

    /// <summary>
    /// Check if an entity is alive (valid).
    /// </summary>
    /// <param name="entity">The entity to check</param>
    /// <returns>True if the entity is alive, false otherwise</returns>
    public bool IsAlive(Entity entity)
    {
        return WorldInteropAPI.IsEntityAlive(_nativeWorldPtr, entity.Id);
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
}
