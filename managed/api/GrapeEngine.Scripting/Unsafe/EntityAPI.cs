/* Start Header *****************************************************************/
/*!
\file   EntityAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
P/Invoke declarations for the Entity API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Entity API.
/// </summary>
internal static partial class EntityAPI
{
    // Use LibraryImportAttribute for better performance
    // See: https://learn.microsoft.com/en-us/dotnet/standard/native-interop/pinvoke-source-generation

    //[LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetComponent")]
    //[UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    //[return: MarshalAs(UnmanagedType.Bool)]
    //public static unsafe partial bool GetComponent(ulong entityId, uint typeHash, byte* outBuffer, int bufferSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetComponentPtr")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial byte* GetComponentPtr(ulong entityId, uint typeHash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_AddComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool AddComponent(ulong entityId, uint typeHash, void* componentData, int dataSize, void* outBuffer);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_SetComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetComponent(ulong entityId, uint typeHash, void* componentData, int dataSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_HasComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static partial bool HasComponent(ulong entityId, uint typeHash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_RemoveComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial void RemoveComponent(ulong entityId, uint typeHash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_DestroyEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial void DestroyEntity(ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_CreateEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial ulong CreateEntity();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsAlive")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static partial bool IsAlive(ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_SetWorld")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetWorld(void* world);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_GetComponent", CallingConvention = CallingConvention.Cdecl)]
    //public static extern unsafe bool GetComponent(ulong entityId, uint typeHash, byte* outBuffer, int bufferSize);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_AddComponent", CallingConvention = CallingConvention.Cdecl)]
    //public static extern unsafe bool AddComponent(ulong entityId, uint typeHash, void* componentData, int dataSize, void* outBuffer);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_SetComponent", CallingConvention = CallingConvention.Cdecl)]
    //public static extern unsafe void SetComponent(ulong entityId, uint typeHash, void* componentData, int dataSize);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_HasComponent", CallingConvention = CallingConvention.Cdecl)]
    //public static extern bool HasComponent(ulong entityId, uint typeHash);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_RemoveComponent", CallingConvention = CallingConvention.Cdecl)]
    //public static extern void RemoveComponent(ulong entityId, uint typeHash);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_DestroyEntity", CallingConvention = CallingConvention.Cdecl)]
    //public static extern void DestroyEntity(ulong entityId);

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_CreateEntity", CallingConvention = CallingConvention.Cdecl)]
    //public static extern ulong CreateEntity();

    //[DllImport(NativeLibHelper.NativeLib, EntryPoint = "ScriptAPI_SetWorld", CallingConvention = CallingConvention.Cdecl)]
    //public static extern unsafe void SetWorld(void* world);
}
