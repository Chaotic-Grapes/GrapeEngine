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

namespace GrapeEngine.Scripting.Components;

/// <summary>
/// Name component: String identifier for an entity.
/// Uses fixed-size char array for unmanaged memory layout.
/// </summary>
[Component]
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
[Component]
[StructLayout(LayoutKind.Sequential)]
public record struct TagMask(uint Mask);

/// <summary>
/// Active component: Whether entity is active/enabled.
/// </summary>
[Component]
[StructLayout(LayoutKind.Sequential)]
public record struct Active(bool Enabled);

/// <summary>
/// Prefab instance metadata component: Runtime data for prefab instances.
/// Contains the hash of the source prefab and instance flags.
/// </summary>
[Component]
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

