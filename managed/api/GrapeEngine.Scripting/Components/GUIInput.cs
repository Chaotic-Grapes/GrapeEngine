using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


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
