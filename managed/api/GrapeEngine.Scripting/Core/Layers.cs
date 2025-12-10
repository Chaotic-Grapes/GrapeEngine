using GrapeEngine.Scripting.Unsafe;
using GrapeEngine.Scripting.Components.Core;

namespace GrapeEngine.Scripting.Core;

public static class Layers
{
    public static LayerMask GetMask(ushort layerId)
    {
        uint m = LayerAPI.GetLayerMask(layerId);
        return new LayerMask(m);
    }

    public static void SetMask(ushort layerId, LayerMask mask)
    {
        LayerAPI.SetLayerMask(layerId, mask);
    }

    public static void SetCollisionBetween(ushort a, ushort b, bool enabled)
    {
        LayerAPI.SetCollisionBetween(a, b, (byte)(enabled ? 1 : 0));
    }

    public static int IdOf(string name)
    {
        return LayerAPI.IdOf(name);
    }

    public static (ushort id, string name)[] ListLayers()
    {
        ushort count = LayerAPI.GetLayerCount();
        var outArr = new (ushort, string)[count];
        const int BUF_SIZE = 256;
        
        for (ushort i = 0; i < count; ++i) {
            var id = LayerAPI.GetLayerIdAtIndex(i);
            
            // Allocate unmanaged buffer and let native write into it
            var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(BUF_SIZE);
            try 
            {
                LayerAPI.GetLayerNameAtIndex(i, buf, BUF_SIZE);
                string name = System.Runtime.InteropServices.Marshal.PtrToStringAnsi(buf) ?? string.Empty;
                outArr[i] = ((ushort)id, name);
            }
            finally 
            {
                System.Runtime.InteropServices.Marshal.FreeHGlobal(buf);
            }
        }

        return outArr;
    }
}
