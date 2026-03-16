using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct LinearVelocity2D
{
    public Vector2 Value;

    public LinearVelocity2D(Vector2 value) => Value = value;
    public LinearVelocity2D(float x, float y) => Value = new Vector2(x, y);
}
