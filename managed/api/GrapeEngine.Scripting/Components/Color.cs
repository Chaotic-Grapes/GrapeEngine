using System.Runtime.InteropServices;
using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public struct Color
{
    public float R, G, B, A;

    public Color(float r, float g, float b, float a = 1f)
    {
        R = r;
        G = g;
        B = b;
        A = a;
    }

    public Color(byte r, byte g, byte b, byte a = 255)
    {
        R = r / 255f;
        G = g / 255f;
        B = b / 255f;
        A = a / 255f;
    }

    public static Color White => new(1, 1, 1, 1);
    public static Color Black => new(0, 0, 0, 1);
    public static Color Red => new(1, 0, 0, 1);
    public static Color Green => new(0, 1, 0, 1);
    public static Color Blue => new(0, 0, 1, 1);
    public static Color Yellow => new(1, 1, 0, 1);
    public static Color Cyan => new(0, 1, 1, 1);
    public static Color Magenta => new(1, 0, 1, 1);

    public (byte, byte, byte, byte) ToHexColor()
        => (
            (byte)(GMath.Clamp(R, 0f, 1f) * 255f),
            (byte)(GMath.Clamp(G, 0f, 1f) * 255f),
            (byte)(GMath.Clamp(B, 0f, 1f) * 255f),
            (byte)(GMath.Clamp(A, 0f, 1f) * 255f)
        );
}
