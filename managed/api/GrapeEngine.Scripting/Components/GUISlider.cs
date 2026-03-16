using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


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
    public uint TrackTextureId;
    public StringId TrackTexturePathId;
    public byte TrackTextureFilter;
    public uint FillTextureId;
    public StringId FillTexturePathId;
    public byte FillTextureFilter;
    public uint KnobTextureId;
    public StringId KnobTexturePathId;
    public byte KnobTextureFilter;
    public float CornerRadius;
    public Vector2 KnobSize;
    public Vector4 Padding;
    public float RotationDegrees;
    public bool Horizontal;
    public bool Disabled;
    public bool ValueChanged;
}
