using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

internal static partial class LayerAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_GetMask")]
    internal static partial uint GetLayerMask(ushort layerId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_SetMask")]
    internal static partial void SetLayerMask(ushort layerId, uint mask);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_SetCollisionBetween")]
    internal static partial void SetCollisionBetween(ushort a, ushort b, byte enabled);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_Count")]
    internal static partial ushort GetLayerCount();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_GetIdAtIndex")]
    internal static partial int GetLayerIdAtIndex(ushort index);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_GetNameAtIndex")]
    internal static partial int GetLayerNameAtIndex(ushort index, System.IntPtr outBuf, int bufSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_IdOf")]
    internal static partial int IdOf([MarshalAs(UnmanagedType.LPStr)] string name);
}
