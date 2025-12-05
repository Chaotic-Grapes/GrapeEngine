/* Start Header *****************************************************************/
/*!
\file   Entity.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for ECS Entity. Provides component access from C# scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine;

/// <summary>
/// Represents an entity in the ECS World. Entities are containers for components.
/// </summary>
public class Entity
{
    private readonly World _world;
    private readonly ulong _id;

    /// <summary>
    /// Internal constructor. Entities are created via World.CreateEntity().
    /// </summary>
    internal Entity(World world, ulong id)
    {
        _world = world;
        _id = id;
    }

    /// <summary>
    /// The unique identifier for this entity.
    /// </summary>
    public ulong Id => _id;

    /// <summary>
    /// The unique identifier for this entity (alias for Id).
    /// </summary>
    public ulong EntityId => _id;

    /// <summary>
    /// Check if this entity is alive (valid).
    /// </summary>
    public bool IsAlive => _world.IsAlive(this);

    /// <summary>
    /// Create an Entity wrapper from a world and entity ID.
    /// </summary>
    public static Entity FromId(World world, ulong entityId)
    {
        return new Entity(world, entityId);
    }

    /// <summary>
    /// Create an Entity wrapper from a world pointer and entity ID (internal use).
    /// </summary>
    internal static unsafe Entity FromId(void* worldPtr, ulong entityId)
    {
        return new Entity(new World(worldPtr), entityId);
    }

    // ============================================================================
    // Component Operations
    // ============================================================================

    /// <summary>
    /// Check if this entity has a component of type T.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    /// <returns>True if component exists, false otherwise</returns>
    public unsafe bool HasComponent<T>() where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        return WorldAPI.HasComponent(_world.NativePtr, _id, typeHash);
    }

    /// <summary>
    /// Get a reference to a component on this entity.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    /// <returns>Reference to the component data</returns>
    /// <exception cref="InvalidOperationException">If component doesn't exist</exception>
    public unsafe ref T GetComponent<T>() where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        void* componentPtr = WorldAPI.GetComponentPtr(_world.NativePtr, _id, typeHash);
        
        if (componentPtr == null)
        {
            throw new InvalidOperationException($"Entity {_id} does not have component {typeof(T).Name}");
        }

        return ref *(T*)componentPtr;
    }

    /// <summary>
    /// Try to get a reference to a component on this entity.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    /// <param name="component">Output reference to component if found</param>
    /// <returns>True if component exists, false otherwise</returns>
    public unsafe bool TryGetComponent<T>(out T component) where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        void* componentPtr = WorldAPI.GetComponentPtr(_world.NativePtr, _id, typeHash);
        
        if (componentPtr == null)
        {
            component = default;
            return false;
        }

        component = *(T*)componentPtr;
        return true;
    }

    /// <summary>
    /// Add a component to this entity.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    /// <param name="component">Initial component data</param>
    /// <returns>Reference to the added component</returns>
    public unsafe ref T AddComponent<T>(T component = default) where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        int size = Marshal.SizeOf<T>();
        
        void* componentData = &component;
        void* addedPtr = WorldAPI.AddComponent(_world.NativePtr, _id, typeHash, componentData, size);
        
        if (addedPtr == null)
        {
            throw new InvalidOperationException($"Failed to add component {typeof(T).Name} to entity {_id}");
        }

        return ref *(T*)addedPtr;
    }

    /// <summary>
    /// Set/update a component's data on this entity.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    /// <param name="component">The new component data</param>
    public void SetComponent<T>(T component) where T : unmanaged
    {
        if (HasComponent<T>())
        {
            ref T existing = ref GetComponent<T>();
            existing = component;
        }
        else
        {
            AddComponent(component);
        }
    }

    /// <summary>
    /// Remove a component from this entity.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    public unsafe void RemoveComponent<T>() where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        WorldAPI.RemoveComponent(_world.NativePtr, _id, typeHash);
    }

    /// <summary>
    /// Destroy this entity, removing it from the world.
    /// </summary>
    public void Destroy()
        => DestroyUnsafe();

    internal unsafe void DestroyUnsafe()
    {
        WorldAPI.DestroyEntity(_world.NativePtr, _id);
    }
}

/// <summary>
/// Helper class for component type hashing (matches C++ FNV-1a implementation).
/// </summary>
internal static class ComponentTypeHelper
{
    private static readonly Dictionary<Type, uint> _typeHashCache = new();

    public static uint GetTypeHash<T>()
    {
        Type type = typeof(T);
        
        if (_typeHashCache.TryGetValue(type, out uint hash))
        {
            return hash;
        }

        // FNV-1a hash algorithm - must match C++ implementation
        string typeName = type.FullName ?? type.Name;
        hash = FNV1aHash(typeName);
        
        _typeHashCache[type] = hash;
        return hash;
    }

    private static uint FNV1aHash(string str)
    {
        uint hash = 2166136261u;
        foreach (char c in str)
        {
            hash ^= c;
            hash *= 16777619u;
        }
        return hash;
    }
}
