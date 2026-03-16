using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public record struct BoxCollider2D
{
    public Vector2 HalfExtents;
    public Vector2 Offset;
    public float Rotation;
    public LayerMask LayerMask;
    public uint Flags;

    public const uint FLAG_IS_TRIGGER = 1 << 0;

    public bool IsTrigger
    {
        readonly get => (Flags & FLAG_IS_TRIGGER) != 0;
        set => Flags = value ? (Flags | FLAG_IS_TRIGGER) : (Flags & ~FLAG_IS_TRIGGER);
    }
}
