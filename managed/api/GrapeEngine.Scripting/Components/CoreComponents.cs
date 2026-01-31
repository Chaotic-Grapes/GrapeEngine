/* Start Header *****************************************************************/
/*!
\file   CoreComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Core ECS component types used in the engine scripting API.
Pure data components using record structs for immutability and value semantics.
*/
/* End Header *******************************************************************/

using GrapeEngine.Math;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Components;

/// <summary>
/// Name component: String identifier for an entity.
/// Uses StringId for unmanaged, blittable storage.
/// 
/// Usage:
/// <code>
/// entity.AddComponent(new Name(Strings.Intern("Player")));
/// string name = Strings.Resolve(entity.GetComponent&lt;Name&gt;().Value);
/// </code>
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Name
{
    /// <summary>
    /// The interned string identifier.
    /// Use Strings.Intern() to create, Strings.Resolve() to read.
    /// </summary>
    public StringId Value;

    /// <summary>
    /// Create a Name component with an interned string.
    /// </summary>
    public Name(StringId value)
    {
        Value = value;
    }

    public override readonly string ToString() => Strings.Resolve(Value) ?? "<null>";
}

/// <summary>
/// Tag mask component: Bitmask for quick entity classification.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct TagMask(uint Mask);

/// <summary>
/// Active component: Whether entity is active/enabled.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Active(bool Enabled);

/// <summary>
/// Parent component: Represents the parent-child relationship.
/// This component is automatically managed when using World.Attach() or Entity.AttachTo().
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Parent
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
    public readonly bool IsValid => ParentEntityId != ulong.MaxValue;

    public override readonly string ToString() => $"Parent({ParentEntityId})";
}

/// <summary>
/// Prefab instance metadata component: Runtime data for prefab instances.
/// Contains the hash of the source prefab and instance flags.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct PrefabInstanceMetadata
{
    public uint PrefabHash;
    public uint Flags;

    public PrefabInstanceMetadata(uint hash, uint flags = 0)
    {
        PrefabHash = hash;
        Flags = flags;
    }
}

