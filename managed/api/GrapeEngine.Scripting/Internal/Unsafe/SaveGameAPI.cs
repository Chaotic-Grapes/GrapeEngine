using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

internal static partial class SaveGameAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_SaveSlot", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    public static partial bool SaveSlot(uint slot, string displayName, string userDataJson);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_LoadSlot")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    public static partial bool LoadSlot(uint slot);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_DeleteSlot")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    public static partial bool DeleteSlot(uint slot);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_HasSlot")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    public static partial bool HasSlot(uint slot);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_SetActiveProfile", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    public static partial bool SetActiveProfile(string profileId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_GetActiveProfile")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int GetActiveProfile(byte* buffer, int bufferSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_GetSlotsJson")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int GetSlotsJson(byte* buffer, int bufferSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_SaveGame_GetSlotJson")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int GetSlotJson(uint slot, byte* buffer, int bufferSize);
}
