using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

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

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_IsRenderEnabled")]
    internal static partial byte IsRenderEnabled(ushort layerId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_IsUpdateEnabled")]
    internal static partial byte IsUpdateEnabled(ushort layerId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_IsPhysicsEnabled")]
    internal static partial byte IsPhysicsEnabled(ushort layerId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_IsVisible")]
    internal static partial byte IsVisible(ushort layerId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Layers_IsLocked")]
    internal static partial byte IsLocked(ushort layerId);
}
