using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


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
