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

using GrapeEngine.Numerics;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components.Core;

// ============================================================================
// Transform & Physics Components
// ============================================================================

/// <summary>
/// Transform component: Position, rotation, and scale of an entity.
/// Immutable by default with value semantics - use `with` expressions to modify.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Transform(Vector3 Position, Quaternion Rotation, Vector3 Scale)
{
    /// <summary>Default transform: identity position, rotation, and uniform scale of 1.</summary>
    public static readonly Transform Identity = new(Vector3.Zero, Quaternion.Identity, Vector3.One);

    /// <summary>Get the forward direction vector based on rotation.</summary>
    public readonly Vector3 Forward => Vector3.Transform(Vector3.UnitZ, Rotation);

    /// <summary>Get the right direction vector based on rotation.</summary>
    public readonly Vector3 Right => Vector3.Transform(Vector3.UnitX, Rotation);

    /// <summary>Get the up direction vector based on rotation.</summary>
    public readonly Vector3 Up => Vector3.Transform(Vector3.UnitY, Rotation);
}

/// <summary>
/// Velocity component: Linear velocity of an entity.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Velocity(Vector3 Value);

/// <summary>
/// Angular velocity component: Rotational velocity of an entity.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct AngularVelocity(Vector3 Value);

// ============================================================================
// Health & Status Components
// ============================================================================

/// <summary>
/// Health component: Current and maximum health values.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Health(float Current, float Max)
{
    /// <summary>Get health as a percentage (0-1).</summary>
    public readonly float Percentage => Current / Max;

    /// <summary>Check if entity is dead.</summary>
    public readonly bool IsDead => Current <= 0f;

    /// <summary>Check if entity is at full health.</summary>
    public readonly bool IsFullHealth => Current >= Max;
}

// ============================================================================
// Input & Control Components
// ============================================================================

/// <summary>
/// Player input component: Current input state for this frame.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct PlayerInput(
    bool MoveForward,
    bool MoveBackward,
    bool MoveLeft,
    bool MoveRight,
    bool Jump,
    Vector2 LookDelta
);

// ============================================================================
// Identification & Management Components
// ============================================================================

/// <summary>
/// Name component: String identifier for an entity.
/// Uses fixed-size char array for unmanaged memory layout.
/// </summary>
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public unsafe struct Name
{
    private fixed char _value[64];

    public Name(string name)
    {
        fixed (char* ptr = _value)
        {
            var chars = name?.ToCharArray() ?? Array.Empty<char>();
            var copyLen = Math.Min(chars.Length, 63);
            for (int i = 0; i < copyLen; i++)
                ptr[i] = chars[i];
            ptr[copyLen] = '\0';
        }
    }

    public readonly override string ToString()
    {
        fixed (char* ptr = _value)
        {
            return new string(ptr).TrimEnd('\0');
        }
    }
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
/// Prefab link component: Reference to the source prefab of this entity.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct PrefabLink
{
    private fixed char _prefabPath[256];

    public PrefabLink(string path)
    {
        fixed (char* ptr = _prefabPath)
        {
            var chars = path?.ToCharArray() ?? Array.Empty<char>();
            var copyLen = Math.Min(chars.Length, 255);
            for (int i = 0; i < copyLen; i++)
                ptr[i] = chars[i];
            ptr[copyLen] = '\0';
        }
    }

    public readonly string GetPath()
    {
        fixed (char* ptr = _prefabPath)
        {
            return new string(ptr).TrimEnd('\0');
        }
    }
}

/// <summary>
/// Lifetime component: Time remaining before entity is destroyed.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Lifetime(float Time);
