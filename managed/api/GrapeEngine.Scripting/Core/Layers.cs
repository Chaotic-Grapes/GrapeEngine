using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Components;

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

    /// <summary>
    /// Check if a layer has rendering enabled.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if rendering is enabled for this layer</returns>
    public static bool IsRenderEnabled(ushort layerId)
    {
        return LayerAPI.IsRenderEnabled(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer has updates enabled.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if updates are enabled for this layer</returns>
    public static bool IsUpdateEnabled(ushort layerId)
    {
        return LayerAPI.IsUpdateEnabled(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer has physics enabled.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if physics is enabled for this layer</returns>
    public static bool IsPhysicsEnabled(ushort layerId)
    {
        return LayerAPI.IsPhysicsEnabled(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer is visible in the editor.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if the layer is visible in the editor</returns>
    public static bool IsVisible(ushort layerId)
    {
        return LayerAPI.IsVisible(layerId) != 0;
    }

    /// <summary>
    /// Check if a layer is locked in the editor.
    /// </summary>
    /// <param name="layerId">The layer ID</param>
    /// <returns>True if the layer is locked in the editor</returns>
    public static bool IsLocked(ushort layerId)
    {
        return LayerAPI.IsLocked(layerId) != 0;
    }
}
