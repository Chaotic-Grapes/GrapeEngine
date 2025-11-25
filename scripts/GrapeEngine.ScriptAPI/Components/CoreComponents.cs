/* Start Header *****************************************************************/
/*!
\file   CoreComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Core ECS component types used in the engine scripting API.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;
using GrapeEngine.Math;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;

public interface IComponentData
{
    void AddToEntity(Entity entity);
}

public readonly struct ComponentData<T>(T component) : IComponentData where T : unmanaged
{
    private readonly T _component = component;

    public void AddToEntity(Entity entity)
        => entity.AddComponent(_component);
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct Name
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

[StructLayout(LayoutKind.Sequential)]
public struct TagMask
{
    public uint Mask;
}

[StructLayout(LayoutKind.Sequential)]
public struct Active
{
    public bool Enabled;
}

[StructLayout(LayoutKind.Sequential)]
public struct PrefabLink
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

[StructLayout(LayoutKind.Sequential)]
public struct Lifetime
{
    public float Time;
}
