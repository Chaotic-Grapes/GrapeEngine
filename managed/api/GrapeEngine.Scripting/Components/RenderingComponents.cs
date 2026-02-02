/* Start Header *****************************************************************/
/*!
\file   RenderingComponents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Rendering and UI-related ECS component types.
*/
/* End Header *******************************************************************/

using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;

[StructLayout(LayoutKind.Sequential)]
public record struct SpriteRenderer2D
{
    public uint TextureId;
    public uint NormalTextureId;
    public Color Color;
    public Vector2 Tiling;
    public Vector2 Offset;
    public int Width;
    public int Height;
    public uint EmissiveTextureId;
    public float EmissiveStrength;
    public StringId TexturePath;
    public StringId NormalTexturePath;
    public StringId EmissiveTexturePath;
}

[StructLayout(LayoutKind.Sequential)]
public record struct SpriteFlip2D
{
    public bool FlipX;
    public bool FlipY;
}

[StructLayout(LayoutKind.Sequential)]
public record struct SpriteShader2D
{
    public bool Bloom;
}

[StructLayout(LayoutKind.Sequential)]
public record struct SpriteSheetAnimation2D
{
    public uint TextureId;
    public uint NormalTextureId;
    public int FrameWidth;
    public int FrameHeight;
    public int SheetWidth;
    public int SheetHeight;
    public int StartFrame;
    public int FrameCount;
    public int RowIndex;
    public int RowStartColumn;
    public int RowFrameCount;
    public float FramesPerSecond;
    public bool Loop;
    public bool Playing;
    public bool UseRow;
    public StringId TexturePath;
    public StringId NormalTexturePath;
}

[StructLayout(LayoutKind.Sequential)]
public record struct AnimationState2D
{
    public int CurrentFrame;
    public float TimeAccumulator;
    public bool Finished;
}

[StructLayout(LayoutKind.Sequential)]
public record struct ShapeCircle2D
{
    public float Radius;
    public Vector2 Offset;
    public Color Color;
    public float Thickness;
    public bool Filled;
}

[StructLayout(LayoutKind.Sequential)]
public record struct ShapeBox2D
{
    public Vector2 HalfExtents;
    public Vector2 Offset;
    public Color Color;
    public float Thickness;
    public bool Filled;
}

[StructLayout(LayoutKind.Sequential)]
public record struct ShapeLine2D
{
    public Vector2 A;
    public Vector2 B;
    public Color Color;
    public float Thickness;
}

[StructLayout(LayoutKind.Sequential)]
public record struct ZIndex2D
{
    public short ZOrder;
}

[StructLayout(LayoutKind.Sequential)]
public record struct Light2D
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

