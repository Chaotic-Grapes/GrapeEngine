/* Start Header *****************************************************************/
/*!
\file   RenderingComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Rendering and UI-related ECS component types.
*/
/* End Header *******************************************************************/

using GrapeEngine.Numerics;
using GrapeEngine.Math;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting;

[StructLayout(LayoutKind.Sequential)]
public struct SpriteRenderer2D
{
    public uint TextureId;
    public Color Color;
    public Vector2 Tiling;
    public Vector2 Offset;
    public int Width;
    public int Height;
    public uint EmissiveTextureId;
    public float EmissiveStrength;
}

[StructLayout(LayoutKind.Sequential)]
public struct SpriteFlip2D
{
    public bool FlipX;
    public bool FlipY;
}

[StructLayout(LayoutKind.Sequential)]
public struct SpriteShader2D
{
    public bool Bloom;
}

[StructLayout(LayoutKind.Sequential)]
public struct SpriteSheetAnimation2D
{
    public uint TextureId;
    public int FrameWidth;
    public int FrameHeight;
    public int SheetWidth;
    public int SheetHeight;
    public int StartFrame;
    public int FrameCount;
    public float FramesPerSecond;
    public bool Loop;
    public bool Playing;
}

[StructLayout(LayoutKind.Sequential)]
public struct AnimationState2D
{
    public int CurrentFrame;
    public float TimeAccumulator;
    public bool Finished;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeCircle2D
{
    public float Radius;
    public Vector2 Offset;
    public Color Color;
    public float Thickness;
    public bool Filled;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeBox2D
{
    public Vector2 HalfExtents;
    public Vector2 Offset;
    public Color Color;
    public float Thickness;
    public bool Filled;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeLine2D
{
    public Vector2 A;
    public Vector2 B;
    public Color Color;
    public float Thickness;
}

[StructLayout(LayoutKind.Sequential)]
public struct ZIndex2D
{
    public short ZOrder;
}

[StructLayout(LayoutKind.Sequential)]
public struct Light2D
{
    public enum Type : byte
    {
        Directional = 0,
        Point = 1
    }

    public Type LightType;
    public Vector3 Position;
    public Vector3 Direction;
    public Color Color;
    public float Intensity;
    public float Range;
    public bool CastsShadows;
}

public enum TextAnchor : byte
{
    Absolute = 0,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
}

[StructLayout(LayoutKind.Sequential)]
public struct Text
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
    public char[] Content;
    
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
    public char[] FontPath;
    
    public float PixelSize;
    public Color Color;
    public TextAnchor Anchor;

    public Text(string content, float pixelSize = 24f, string fontPath = "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf")
    {
        Content = new char[256];
        FontPath = new char[128];
        PixelSize = pixelSize;
        Color = new Color(1, 1, 1, 1);
        Anchor = TextAnchor.Absolute;

        if (!string.IsNullOrEmpty(content))
        {
            var chars = content.ToCharArray();
            Array.Copy(chars, Content, (int)GMath.Min(chars.Length, 255));
        }

        if (!string.IsNullOrEmpty(fontPath))
        {
            var chars = fontPath.ToCharArray();
            Array.Copy(chars, FontPath, (int)GMath.Min(chars.Length, 127));
        }
    }

    public readonly string GetContent() => new string(Content).TrimEnd('\0');
    public readonly string GetFontPath() => new string(FontPath).TrimEnd('\0');
}

[StructLayout(LayoutKind.Sequential)]
public struct UIButton
{
    public int ID;
    public float X, Y, W, H;
    public bool Hovered;
    public bool Pressed;
    public uint ActionID;
}
