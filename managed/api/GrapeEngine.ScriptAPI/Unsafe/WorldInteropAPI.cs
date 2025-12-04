/* Start Header *****************************************************************/
/*!
\file   WorldInteropAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for World interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.ScriptAPI.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for World/Entity operations.
/// </summary>
internal static partial class WorldInteropAPI
{
    // ============================================================================
    // Entity Lifecycle
    // ============================================================================

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_CreateEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong CreateEntity(void* worldPtr);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_DestroyEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void DestroyEntity(void* worldPtr, ulong entityId);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_IsEntityAlive")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool IsEntityAlive(void* worldPtr, ulong entityId);

    // ============================================================================
    // Component Operations
    // ============================================================================

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_HasComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool HasComponent(void* worldPtr, ulong entityId, uint componentTypeHash);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_GetComponentPtr")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetComponentPtr(void* worldPtr, ulong entityId, uint componentTypeHash);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_AddComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* AddComponent(void* worldPtr, ulong entityId, uint componentTypeHash, void* componentData, int componentSize);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_RemoveComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void RemoveComponent(void* worldPtr, ulong entityId, uint componentTypeHash);
}
