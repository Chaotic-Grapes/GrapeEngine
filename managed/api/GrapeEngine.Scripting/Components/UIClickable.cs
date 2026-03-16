using System.Runtime.InteropServices;
using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public struct UIClickable
{
    public uint ClickActionID;
    public bool IsClickable;
    public bool IsHovered;
    public bool IsPressed;
}
