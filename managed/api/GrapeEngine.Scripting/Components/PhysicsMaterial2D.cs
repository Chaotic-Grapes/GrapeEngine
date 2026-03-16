using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct PhysicsMaterial2D
{
    public float Friction;
    public float Restitution;
    public float PositionCorrectPercent;
}
