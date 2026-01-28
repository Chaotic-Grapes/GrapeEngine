using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Core;

public static class Layers
{
    public static LayerMask GetMask(ushort layerId)
    {
        uint m = LayerAPI.GetLayerMask(layerId);
        return new LayerMask(m);
    }

    public static void SetMask(ushort layerId, LayerMask mask)
    {
        LayerAPI.SetLayerMask(layerId, mask);
    }

    public static void SetCollisionBetween(ushort a, ushort b, bool enabled)
    {
        LayerAPI.SetCollisionBetween(a, b, (byte)(enabled ? 1 : 0));
    }

    public static int IdOf(string name)
    {
        return LayerAPI.IdOf(name);
    }

    public static (ushort id, string name)[] ListLayers()
    {
        ushort count = LayerAPI.GetLayerCount();
        var outArr = new (ushort, string)[count];
        const int BUF_SIZE = 256;
        
        for (ushort i = 0; i < count; ++i) {
            var id = LayerAPI.GetLayerIdAtIndex(i);
            
            // Allocate unmanaged buffer and let native write into it
            var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(BUF_SIZE);
            try 
            {
                LayerAPI.GetLayerNameAtIndex(i, buf, BUF_SIZE);
                string name = System.Runtime.InteropServices.Marshal.PtrToStringAnsi(buf) ?? string.Empty;
                outArr[i] = ((ushort)id, name);
            }
            finally 
            {
                System.Runtime.InteropServices.Marshal.FreeHGlobal(buf);
            }
        }

        return outArr;
    }

    /// <summary>
    /// Check if a layer has rendering enabled.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if rendering is enabled for this layer</returns>
    public static bool IsRenderEnabled(ushort layerId)
    {
        return LayerAPI.IsRenderEnabled(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer has updates enabled.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if updates are enabled for this layer</returns>
    public static bool IsUpdateEnabled(ushort layerId)
    {
        return LayerAPI.IsUpdateEnabled(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer has physics enabled.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if physics is enabled for this layer</returns>
    public static bool IsPhysicsEnabled(ushort layerId)
    {
        return LayerAPI.IsPhysicsEnabled(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer is visible in the editor.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if the layer is visible in the editor</returns>
    public static bool IsVisible(ushort layerId)
    {
        return LayerAPI.IsVisible(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer is locked in the editor.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if the layer is locked in the editor</returns>
    public static bool IsLocked(ushort layerId)
    {
        return LayerAPI.IsLocked(layerId) != 0;
    }
}

// ============================================================================
// Parent-Child Relationship Components
// ============================================================================

/// <summary>
/// Represents the parent-child relationship component.
/// This component is automatically managed when using World.Attach() or Entity.AttachTo().
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Parent : IEquatable<Parent>
{
    /// <summary>
    /// The ID of the parent entity (encoded as index and generation).
    /// </summary>
    public ulong ParentEntityId;

    /// <summary>
    /// Create a Parent component with the given parent entity ID.
    /// </summary>
    /// <param name="parentId">The parent entity ID</param>
    public Parent(ulong parentId)
    {
        ParentEntityId = parentId;
    }

    /// <summary>
    /// Create a Parent component from a parent entity.
    /// </summary>
    /// <param name="parentEntity">The parent entity</param>
    public Parent(Entity parentEntity)
    {
        ParentEntityId = parentEntity.Id;
    }

    /// <summary>
    /// Check if this parent is valid (not null/invalid entity).
    /// </summary>
    public bool IsValid => ParentEntityId != ulong.MaxValue;

    public override bool Equals(object? obj) => obj is Parent p && Equals(p);
    public bool Equals(Parent other) => ParentEntityId == other.ParentEntityId;
    public override int GetHashCode() => ParentEntityId.GetHashCode();
    public static bool operator ==(Parent a, Parent b) => a.Equals(b);
    public static bool operator !=(Parent a, Parent b) => !a.Equals(b);
}

/// <summary>
/// Represents the Name component for entities.
/// </summary>
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public unsafe struct Name : IEquatable<Name>
{
    /// <summary>
    /// The name value (max 63 characters + null terminator).
    /// </summary>
    private fixed byte _value[64];

    /// <summary>
    /// Get or set the name value.
    /// </summary>
    public string Value
    {
        get
        {
            fixed (byte* ptr = _value)
            {
                return System.Runtime.InteropServices.Marshal.PtrToStringAnsi((IntPtr)ptr) ?? "";
            }
        }
        set
        {
            string str = value?.Substring(0, System.Math.Min(value.Length, 63)) ?? "";
            fixed (byte* ptr = _value)
            {
                byte[] bytes = System.Text.Encoding.ASCII.GetBytes(str);
                System.Runtime.InteropServices.Marshal.Copy(bytes, 0, (IntPtr)ptr, System.Math.Min(bytes.Length, 63));
                ptr[System.Math.Min(bytes.Length, 63)] = 0; // null terminator
            }
        }
    }

    /// <summary>
    /// Create a Name component with the given value.
    /// </summary>
    /// <param name="value">The name string (will be truncated to 63 characters if longer)</param>
    public Name(string value)
    {
        fixed (byte* ptr = _value)
        {
            string str = value?.Substring(0, System.Math.Min(value.Length, 63)) ?? "";
            byte[] bytes = System.Text.Encoding.ASCII.GetBytes(str);
            System.Runtime.InteropServices.Marshal.Copy(bytes, 0, (IntPtr)ptr, System.Math.Min(bytes.Length, 63));
            ptr[System.Math.Min(bytes.Length, 63)] = 0; // null terminator
        }
    }

    public override bool Equals(object? obj) => obj is Name n && Equals(n);
    public bool Equals(Name other) => Value == other.Value;
    public override int GetHashCode() => Value?.GetHashCode() ?? 0;
    public static bool operator ==(Name a, Name b) => a.Equals(b);
    public static bool operator !=(Name a, Name b) => !a.Equals(b);
    public override string ToString() => Value ?? "";
}

// ============================================================================
// Hierarchy Utilities
// ============================================================================

/// <summary>
/// Utility class for working with entity parent-child relationships.
/// </summary>
public static class HierarchyUtils
{
    /// <summary>
    /// Recursively collect all descendants of an entity.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The root entity</param>
    /// <returns>List of all descendants (children, grandchildren, etc.)</returns>
    public static List<Entity> GetAllDescendants(World world, Entity entity)
    {
        var descendants = new List<Entity>();
        CollectDescendantsRecursive(world, entity, descendants);
        return descendants;
    }

    private static void CollectDescendantsRecursive(World world, Entity entity, List<Entity> descendants)
    {
        var children = world.GetChildren(entity);
        foreach (var child in children)
        {
            descendants.Add(child);
            CollectDescendantsRecursive(world, child, descendants);
        }
    }

    /// <summary>
    /// Get the root ancestor of an entity (the top-most parent).
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity</param>
    /// <returns>The root ancestor entity, or the entity itself if it has no parent</returns>
    public static Entity GetRoot(World world, Entity entity)
    {
        var current = entity;
        while (true)
        {
            var parent = world.GetParent(current);
            if (parent == null)
                return current;
            current = parent;
        }
    }

    /// <summary>
    /// Check if an entity is an ancestor of another entity.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="potentialAncestor">The potential ancestor</param>
    /// <param name="entity">The entity to check</param>
    /// <returns>True if potentialAncestor is an ancestor of entity</returns>
    public static bool IsAncestorOf(World world, Entity potentialAncestor, Entity entity)
    {
        var current = entity;
        while (true)
        {
            var parent = world.GetParent(current);
            if (parent == null)
                return false;
            if (parent.Id == potentialAncestor.Id)
                return true;
            current = parent;
        }
    }

    /// <summary>
    /// Get the depth of an entity in the hierarchy (0 if root, 1 if child of root, etc.).
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity</param>
    /// <returns>The depth in the hierarchy</returns>
    public static int GetDepth(World world, Entity entity)
    {
        int depth = 0;
        var current = entity;
        while (true)
        {
            var parent = world.GetParent(current);
            if (parent == null)
                return depth;
            depth++;
            current = parent;
        }
    }

    /// <summary>
    /// Detach an entity and all its descendants from the hierarchy.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity to detach</param>
    public static void DetachHierarchy(World world, Entity entity)
    {
        var descendants = GetAllDescendants(world, entity);
        world.Detach(entity);
        foreach (var descendant in descendants)
        {
            world.Detach(descendant);
        }
    }

    /// <summary>
    /// Move an entity (with all its descendants) to a new parent.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="entity">The entity to move</param>
    /// <param name="newParent">The new parent entity</param>
    public static void Reparent(World world, Entity entity, Entity newParent)
    {
        world.Detach(entity);
        world.Attach(entity, newParent);
    }

    /// <summary>
    /// Find a direct child by name.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="parent">The parent entity to search under</param>
    /// <param name="name">The name to search for</param>
    /// <returns>The found entity, or null if not found</returns>
    public static Entity? FindChild(World world, Entity parent, string name)
    {
        var children = world.GetChildren(parent);
        foreach (var child in children)
        {
            if (child.TryGetComponent<Name>(out var nameComponent) && 
                nameComponent.Value == name)
            {
                return child;
            }
        }
        return null;
    }

    /// <summary>
    /// Recursively find an entity by name in the hierarchy.
    /// </summary>
    /// <param name="world">The world</param>
    /// <param name="parent">The parent entity to search under</param>
    /// <param name="name">The name to search for</param>
    /// <returns>The found entity, or null if not found</returns>
    public static Entity? FindChildRecursive(World world, Entity parent, string name)
    {
        var children = world.GetChildren(parent);
        foreach (var child in children)
        {
            if (child.TryGetComponent<Name>(out var nameComponent) && 
                nameComponent.Value == name)
            {
                return child;
            }
            
            var found = FindChildRecursive(world, child, name);
            if (found != null)
                return found;
        }
        return null;
    }
}

