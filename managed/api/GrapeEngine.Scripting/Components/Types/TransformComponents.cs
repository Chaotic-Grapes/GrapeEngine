/* Start Header *****************************************************************/
/*!
\file   TransformComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Transform-related ECS component types.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;
using GrapeEngine.Scripting.Components.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;

[StructLayout(LayoutKind.Sequential)]
public record struct LocalTransform(Vector3 Position, Quaternion Rotation, Vector3 Scale)
{
    public Vector3 Position = Position;
    public Quaternion Rotation = Rotation;
    public Vector3 Scale = Scale;

    public static LocalTransform Default => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
}

[StructLayout(LayoutKind.Sequential)]
public record struct WorldTransform
{
    public Matrix4x4 Matrix;
    public bool Dirty;
}
