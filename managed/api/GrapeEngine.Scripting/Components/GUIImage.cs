using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


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
