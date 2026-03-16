using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


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
