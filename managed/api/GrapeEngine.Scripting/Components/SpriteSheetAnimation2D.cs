using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct SpriteSheetAnimation2D
{
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
    public StringId TexturePath;
    public StringId NormalTexturePath;
}
