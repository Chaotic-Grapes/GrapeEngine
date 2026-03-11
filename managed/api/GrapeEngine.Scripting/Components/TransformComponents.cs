/* Start Header *****************************************************************/
/*!
\file   TransformComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Transform-related ECS component types.
*/
/* End Header *******************************************************************/

using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;

[StructLayout(LayoutKind.Sequential)]
public record struct Layer
{
    public ushort Id;

    public Layer(ushort id)
    {
        Id = id;
    }
}

[StructLayout(LayoutKind.Sequential)]
public record struct LocalTransform(
    Vector3 Position = default,
    Quaternion Rotation = default,
    Vector3 Scale = default)
{
    public static LocalTransform Default => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
}

[StructLayout(LayoutKind.Sequential)]
public record struct WorldTransform
{
    public Matrix4x4 Matrix;
    public bool Dirty;
}

