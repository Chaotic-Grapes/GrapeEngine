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

namespace GrapeEngine.Scripting.Core;

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
    public bool HasComponent<T>() where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        unsafe
        {
            return WorldAPI.HasComponent(_world.NativePtr, _id, typeHash);
        }
    }

    /// <summary>
    /// Get a reference to a component on this entity.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged struct)</typeparam>
    /// <returns>Reference to the component data</returns>
    /// <exception cref="InvalidOperationException">If component doesn't exist</exception>
    public ref T GetComponent<T>() where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();

        unsafe
        {
            void* nativePtr = _world.NativePtr;
            void* componentPtr = WorldAPI.GetComponentPtr(nativePtr, _id, typeHash);
            
            if (componentPtr == null)
                throw new InvalidOperationException($"Entity {_id} does not have component {typeof(T).Name}");
            
            return ref *(T*)componentPtr;
        }
    }

    /// <summary>
    /// Try to get a reference to a component on this entity.
    /// </summary>
    public bool TryGetComponent<T>(out T component) where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        unsafe
        {
            void* nativePtr = _world.NativePtr;
            void* componentPtr = WorldAPI.GetComponentPtr(nativePtr, _id, typeHash);

            if (componentPtr == null)
            {
                component = default;
                return false;
            }

            component = *(T*)componentPtr;
            return true;
        }
    }

    /// <summary>
    /// Add a component to this entity.
    /// </summary>
    public ref T AddComponent<T>(T component = default) where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        int size = Marshal.SizeOf<T>();
        
        unsafe
        {
            void* nativePtr = _world.NativePtr;
            void* componentData = &component;
            void* addedPtr = WorldAPI.AddComponent(nativePtr, _id, typeHash, componentData, size);
            
            if (addedPtr == null)
                throw new InvalidOperationException($"Failed to add component {typeof(T).Name} to entity {_id}");
            
            return ref *(T*)addedPtr;
        }
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
    public void RemoveComponent<T>() where T : unmanaged
    {
        ComponentRegistry.EnsureRegistered<T>();
        uint typeHash = ComponentTypeHelper.GetTypeHash<T>();
        unsafe
        {
            WorldAPI.RemoveComponent(_world.NativePtr, _id, typeHash);
        }
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

    // ============================================================================
    // Hierarchy Operations
    // ============================================================================

    /// <summary>
    /// Attach this entity as a child to the specified parent entity.
    /// </summary>
    /// <param name="parent">The parent entity</param>
    public void AttachTo(Entity parent)
    {
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));

        unsafe
        {
            WorldAPI.AttachChild(_world.NativePtr, _id, parent._id);
        }
    }

    /// <summary>
    /// Detach this entity from its parent.
    /// </summary>
    public void Detach()
    {
        unsafe
        {
            WorldAPI.DetachChild(_world.NativePtr, _id);
        }
    }

    /// <summary>
    /// Get the parent of this entity, or null if no parent.
    /// </summary>
    /// <returns>The parent entity, or null if this entity has no parent</returns>
    public Entity? GetParent()
    {
        unsafe
        {
            ulong parentId = WorldAPI.GetParent(_world.NativePtr, _id);

            // Check for null/invalid entity (max uint64)
            if (parentId == ulong.MaxValue)
                return null;

            return _world.IsAlive(FromId(_world, parentId))
                ? new Entity(_world, parentId)
                : null;
        }
    }

    /// <summary>
    /// Get the first child of this entity, or null if no children.
    /// </summary>
    /// <returns>The first child entity, or null if this entity has no children</returns>
    public Entity? GetFirstChild()
    {
        unsafe
        {
            ulong childId = WorldAPI.GetFirstChild(_world.NativePtr, _id);

            // Check for null/invalid entity
            if (childId == ulong.MaxValue)
                return null;

            return _world.IsAlive(FromId(_world, childId))
                ? new Entity(_world, childId)
                : null;
        }
    }

    /// <summary>
    /// Get the next sibling of this entity (for iterating children), or null if no more siblings.
    /// </summary>
    /// <returns>The next sibling entity, or null if this is the last child</returns>
    public Entity? GetNextSibling()
    {
        unsafe
        {
            ulong siblingId = WorldAPI.GetNextSibling(_world.NativePtr, _id);

            // Check for null/invalid entity
            if (siblingId == ulong.MaxValue)
                return null;

            return _world.IsAlive(FromId(_world, siblingId))
                ? new Entity(_world, siblingId)
                : null;
        }
    }

    /// <summary>
    /// Get the number of direct children this entity has.
    /// </summary>
    /// <returns>The child count</returns>
    public int GetChildCount()
    {
        unsafe
        {
            return WorldAPI.GetChildCount(_world.NativePtr, _id);
        }
    }

    /// <summary>
    /// Iterate over all direct children of this entity.
    /// </summary>
    /// <param name="action">Action to invoke for each child</param>
    public void ForEachChild(Action<Entity> action)
    {
        if (action == null) throw new ArgumentNullException(nameof(action));

        Entity? child = GetFirstChild();
        while (child != null)
        {
            action(child);
            child = child.GetNextSibling();
        }
    }

    /// <summary>
    /// Get all direct children of this entity as a list.
    /// </summary>
    /// <returns>List of child entities</returns>
    public List<Entity> GetChildren()
    {
        var children = new List<Entity>(GetChildCount());
        ForEachChild(child => children.Add(child));
        return children;
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
