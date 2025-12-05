/* Start Header *****************************************************************/
/*!
\file   WorldAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for World/Entity interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for World/Entity operations.
/// </summary>
internal static partial class WorldAPI
{
    // ============================================================================
    // Entity Lifecycle
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_CreateEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong CreateEntity(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_DestroyEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void DestroyEntity(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_IsEntityAlive")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool IsEntityAlive(void* worldPtr, ulong entityId);

    // ============================================================================
    // Component Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_HasComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool HasComponent(void* worldPtr, ulong entityId, uint componentTypeHash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_GetComponentPtr")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetComponentPtr(void* worldPtr, ulong entityId, uint componentTypeHash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_AddComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* AddComponent(void* worldPtr, ulong entityId, uint componentTypeHash, void* componentData, int componentSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_RemoveComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void RemoveComponent(void* worldPtr, ulong entityId, uint componentTypeHash);
}
