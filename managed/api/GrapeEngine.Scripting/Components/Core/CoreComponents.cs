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

namespace GrapeEngine.Scripting.Components.Core;

/// <summary>
/// Name component: String identifier for an entity.
/// Uses fixed-size char array for unmanaged memory layout.
/// </summary>
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public record struct Name
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
    public char[] Value;

    public Name(string value)
    {
        Value = new char[64];
        var chars = value.ToCharArray();
        Array.Copy(chars, Value, (int)GMath.Min(chars.Length, 63));
    }

    public override readonly string ToString() => new string(Value).TrimEnd('\0');
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
public record struct PrefabLink
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
    public char[] PrefabPath;

    public PrefabLink(string path)
    {
        PrefabPath = new char[256];
        if (!string.IsNullOrEmpty(path))
        {
            var chars = path.ToCharArray();
            Array.Copy(chars, PrefabPath, (int)GMath.Min(chars.Length, 255));
        }
    }

    public readonly string GetPath() => new string(PrefabPath).TrimEnd('\0');
}

/// <summary>
/// Lifetime component: Time remaining before entity is destroyed.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public record struct Lifetime(float Time);
