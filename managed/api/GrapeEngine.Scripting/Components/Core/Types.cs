/* Start Header *****************************************************************/
/*!
\file   Types.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Additional shared types used by components.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Components.Core;

[StructLayout(LayoutKind.Sequential)]
public struct Layer
{
    public ushort Id;
}

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

[StructLayout(LayoutKind.Sequential)]
public struct Matrix4x4
{
    // Row-major 4x4 matrix
    public float M11, M12, M13, M14;
    public float M21, M22, M23, M24;
    public float M31, M32, M33, M34;
    public float M41, M42, M43, M44;
}

[StructLayout(LayoutKind.Sequential)]
public struct UIClickable
{
    public uint ClickActionID;
    public bool IsClickable;
    public bool IsHovered;
    public bool IsPressed;
}
