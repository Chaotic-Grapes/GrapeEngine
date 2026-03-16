using System.Runtime.InteropServices;
using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public struct Matrix4x4
{
    // Row-major 4x4 matrix
    public float M11, M12, M13, M14;
    public float M21, M22, M23, M24;
    public float M31, M32, M33, M34;
    public float M41, M42, M43, M44;
}
