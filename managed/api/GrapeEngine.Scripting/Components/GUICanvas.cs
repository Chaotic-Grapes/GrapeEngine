using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct GUICanvas
{
    public Vector2 ReferenceSize;
    public Vector2 Offset;
    public GUIScaleMode ScaleMode;
}
