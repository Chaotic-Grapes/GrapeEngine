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
    public int Row;
    public int FrameOffset;
    public int FrameLength;
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

[StructLayout(LayoutKind.Sequential)]
public record struct GUICanvas
{
    public Vector2 ReferenceSize;
    public Vector2 Offset;
    public GUIScaleMode ScaleMode;
}

public enum GUIScaleMode : byte
{
    Fit = 0,
    Fill = 1,
    MatchWidth = 2,
    MatchHeight = 3
}

public enum GUIRenderSpace : byte
{
    Screen = 0,
    World = 1
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIRenderMode
{
    public GUIRenderSpace Space;
}

public enum GUIAlignment : byte
{
    TopLeft = 0,
    Top = 1,
    TopRight = 2,
    Left = 3,
    Center = 4,
    Right = 5,
    BottomLeft = 6,
    Bottom = 7,
    BottomRight = 8
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIElement
{
    public Vector2 Position;
    public Vector2 Size;
    public bool Visible;
    public GUIAlignment Alignment;
    public short ZOrder;
    public Vector4 Margin;
    public Vector4 Padding;
    public Vector2 ResolvedPosition;
    public Vector2 ResolvedSize;
    public Vector2 ContentPosition;
    public Vector2 ContentSize;
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIPanel
{
    public Color Color;
    public float CornerRadius;
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIText
{
    public StringId TextId;
    public StringId FontPathId;
    public Color Color;
    public float FontSize;
    public bool Wrap;
    public GUITextHAlign HAlign;
    public GUITextVAlign VAlign;
}

public enum GUITextHAlign : byte
{
    Left = 0,
    Center = 1,
    Right = 2
}

public enum GUITextVAlign : byte
{
    Top = 0,
    Middle = 1,
    Bottom = 2
}

public enum GUIImageScaleMode : byte
{
    Stretch = 0,
    Fit = 1,
    Fill = 2
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIImage
{
    public uint TextureId;
    public StringId TexturePathId;
    public Color Color;
    public Vector4 UVRect;
    public GUIImageScaleMode ScaleMode;
    public bool UseSlicing;
    public Vector4 SliceBorder;
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIInput
{
    public bool Hovered;
    public bool Pressed;
    public bool Clicked;
    public bool Released;
    public bool Dragging;
    public bool Entered;
    public bool Exited;
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIStateStyle
{
    public Color NormalColor;
    public Color HoverColor;
    public Color PressedColor;
    public Color DisabledColor;
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUIButton
{
    public StringId TextId;
    public StringId FontPathId;
    public StringId IconPathId;
    public Color TextColor;
    public Color IconColor;
    public float FontSize;
    public float CornerRadius;
    public Vector2 IconSize;
    public Vector2 IconOffset;
    public Vector4 Padding;
    public bool Disabled;
    public bool Toggle;
    public bool Toggled;
}

[StructLayout(LayoutKind.Sequential)]
public record struct GUISlider
{
    public float Value;
    public float Min;
    public float Max;
    public float Step;
    public Color TrackColor;
    public Color FillColor;
    public Color KnobColor;
    public float CornerRadius;
    public Vector2 KnobSize;
    public Vector4 Padding;
    public bool Horizontal;
    public bool Disabled;
    public bool ValueChanged;
}

