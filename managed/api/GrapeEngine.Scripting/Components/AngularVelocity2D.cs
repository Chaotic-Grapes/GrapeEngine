using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct AngularVelocity2D
{
    public float Value;

    public AngularVelocity2D(float value) => Value = value;
}
