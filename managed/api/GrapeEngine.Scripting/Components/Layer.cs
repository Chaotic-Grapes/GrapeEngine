using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct Layer
{
    public ushort Id;

    public Layer(ushort id)
    {
        Id = id;
    }
}
