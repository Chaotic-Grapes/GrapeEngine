using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct Acceleration2D
{
    public Vector2 Value;

    public Acceleration2D(Vector2 value) => Value = value;
    public Acceleration2D(float x, float y) => Value = new Vector2(x, y);
}
