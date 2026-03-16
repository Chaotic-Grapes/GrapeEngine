using GrapeEngine.Math;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Components;


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
