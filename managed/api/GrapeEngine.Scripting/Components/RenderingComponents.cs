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
    // Keep field order/layout in sync with ECS::Components::SpriteRenderer2D.
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
    public byte TextureFilter;
    public byte _paddingFilter0;
    public byte _paddingFilter1;
    public byte _paddingFilter2;
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
    // Keep field order/layout in sync with ECS::Components::SpriteSheetAnimation2D.
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
    public bool UseSegments;
    public byte SegmentCount;
    public byte _paddingSegments0;
    public byte _paddingSegments1;

    // SegmentRows[8]
    public int SegmentRow0;
    public int SegmentRow1;
    public int SegmentRow2;
    public int SegmentRow3;
    public int SegmentRow4;
    public int SegmentRow5;
    public int SegmentRow6;
    public int SegmentRow7;

    // SegmentOffsets[8]
    public int SegmentOffset0;
    public int SegmentOffset1;
    public int SegmentOffset2;
    public int SegmentOffset3;
    public int SegmentOffset4;
    public int SegmentOffset5;
    public int SegmentOffset6;
    public int SegmentOffset7;

    // SegmentLengths[8]
    public int SegmentLength0;
    public int SegmentLength1;
    public int SegmentLength2;
    public int SegmentLength3;
    public int SegmentLength4;
    public int SegmentLength5;
    public int SegmentLength6;
    public int SegmentLength7;

    public StringId TexturePath;
    public StringId NormalTexturePath;
    public byte TextureFilter;
    public byte _paddingFilter0;
    public byte _paddingFilter1;
    public byte _paddingFilter2;
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
public record struct TileMapComponent
{
    public StringId TileMapPath;
    public StringId TilesetTexturePath;
    public float TileWorldSize;
    public uint TilePixelSize;
    public uint DefaultWidth;
    public uint DefaultHeight;
    public uint LayerIndex;
    public bool Visible;
}

[StructLayout(LayoutKind.Sequential)]
public record struct ParticleEmitter
{
    public uint PresetId;
    public int MaxParticles;
    public float EmissionRate;
    public int BurstCount;
    public float ParticleSize;
    public bool Active;

    public uint TextureId;
    public StringId TexturePath;

    public float SpeedMin;
    public float SpeedMax;
    public float GravityX;
    public float GravityY;
    public float Drag;
    public float Turbulence;
    public float WobbleFrequency;
    public float WobbleAmplitude;
    public float SizeStart;
    public float SizeEnd;
    public float LifetimeMin;
    public float LifetimeMax;
    public float EmissionAngle;
    public float EmissionSpread;
    public float EmissionRadius;
    public byte EmissionShape;
    public float ColorStartR;
    public float ColorStartG;
    public float ColorStartB;
    public float ColorStartA;
    public float ColorEndR;
    public float ColorEndG;
    public float ColorEndB;
    public float ColorEndA;
    public bool DieOnCollision;
    public float Bounciness;
    public bool KillOutOfBounds;
    public float RotationSpeedMin;
    public float RotationSpeedMax;
}

[StructLayout(LayoutKind.Sequential)]
public record struct BoidFlock
{
    // Keep field order/layout in sync with ECS::Components::BoidFlock.
    public int Count;
    public float SeparationWeight;
    public float AlignmentWeight;
    public float CohesionWeight;
    public float CollisionAvoidWeight;
    public float CollisionAvoidRadius;
    public float VisualRange;
    public float MaxSpeed;
    public float MaxForce;
    public float BoidSize;
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
        public Vector2 AnchorMin;
        public Vector2 AnchorMax;
        public Vector2 ResolvedPosition;
        public Vector2 ResolvedSize;
        public Vector2 ContentPosition;
        public Vector2 ContentSize;
        public Vector2 ScreenPosition;
        public Vector2 ScreenSize;
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
    // Keep field order/layout in sync with ECS::Components::GUIText.
    public StringId TextId;
    public StringId FontPathId;
    public Color Color;
    public float FontSize;
    public bool Wrap;
    public GUIAlignment Alignment;
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
    // Keep field order/layout in sync with ECS::Components::GUIImage.
    public uint TextureId;
    public StringId TexturePathId;
    public Color Color;
    public Vector4 UVRect;
    public GUIImageScaleMode ScaleMode;
    public byte TextureFilter;
    public bool UseSlicing;
    public byte _paddingFilter;
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
    // Keep field order/layout in sync with ECS::Components::GUISlider.
    public float Value;
    public float Min;
    public float Max;
    public float Step;
    public Color TrackColor;
    public Color FillColor;
    public Color KnobColor;
    public uint TrackTextureId;
    public StringId TrackTexturePathId;
    public Vector4 TrackUVRect;
    public byte TrackTextureFilter;
    public uint FillTextureId;
    public StringId FillTexturePathId;
    public Vector4 FillUVRect;
    public byte FillTextureFilter;
    public uint KnobTextureId;
    public StringId KnobTexturePathId;
    public Vector4 KnobUVRect;
    public byte KnobTextureFilter;
    public float CornerRadius;
    public Vector2 KnobSize;
    public Vector4 Padding;
    public bool Horizontal;
    public bool Disabled;
    public bool ValueChanged;
}

