using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct LocalTransform(
    Vector3 Position = default,
    Quaternion Rotation = default,
    Vector3 Scale = default)
{
    public static LocalTransform Default => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
}
