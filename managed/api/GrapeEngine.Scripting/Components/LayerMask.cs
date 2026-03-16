using System.Runtime.InteropServices;
using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Components;


[StructLayout(LayoutKind.Sequential)]
public readonly struct LayerMask
{
    public readonly uint Mask;

    public LayerMask(uint mask) => Mask = mask;

    public static LayerMask All => new LayerMask(0xFFFFFFFFu);
    public static LayerMask None => new LayerMask(0u);

    public static LayerMask FromLayer(ushort id)
    {
        if (id >= 32) return None; // out-of-range -> None
        return new LayerMask(1u << id);
    }

    public static LayerMask FromLayers(params ushort[] ids)
    {
        uint m = 0u;
        if (ids != null)
        {
            foreach (var id in ids)
            {
                if (id < 32) m |= (1u << id);
            }
        }
        return new LayerMask(m);
    }

    public bool Contains(ushort layerId)
    {
        return layerId < 32 && ((Mask & (1u << layerId)) != 0u);
    }

    public LayerMask WithLayer(ushort layerId)
    {
        if (layerId >= 32) return this;
        return new LayerMask(Mask | (1u << layerId));
    }

    public LayerMask WithoutLayer(ushort layerId)
    {
        if (layerId >= 32) return this;
        return new LayerMask(Mask & ~(1u << layerId));
    }

    public static LayerMask operator |(LayerMask a, LayerMask b) => new LayerMask(a.Mask | b.Mask);
    public static LayerMask operator &(LayerMask a, LayerMask b) => new LayerMask(a.Mask & b.Mask);
    public static LayerMask operator ~(LayerMask a) => new LayerMask(~a.Mask);

    public static implicit operator uint(LayerMask lm) => lm.Mask;
    public static implicit operator LayerMask(uint v) => new LayerMask(v);

    public override string ToString() => $"LayerMask(0x{Mask:X8})";
}
