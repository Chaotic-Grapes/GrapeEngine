using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct ShapeLine2D
{
    public Vector2 A;
    public Vector2 B;
    public Color Color;
    public float Thickness;
}
