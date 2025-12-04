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
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;

[StructLayout(LayoutKind.Sequential)]
public struct Rotator
{
    public float RotationSpeed;
    public float RotationOffset;

    public Rotator(float speed, float offset = 0f)
    {
        RotationSpeed = speed;
        RotationOffset = offset;
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct LocalTransform
{
    public Vector3 Position;
    public Quaternion Rotation;
    public Vector3 Scale;

    public LocalTransform(Vector3 position, Quaternion rotation, Vector3 scale)
    {
        Position = position;
        Rotation = rotation;
        Scale = scale;
    }

    public static LocalTransform Default => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
}

[StructLayout(LayoutKind.Sequential)]
public struct WorldTransform
{
    public Matrix4x4 Matrix;
    public bool Dirty;
}
