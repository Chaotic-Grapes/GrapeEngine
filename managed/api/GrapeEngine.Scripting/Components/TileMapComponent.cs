using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


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
