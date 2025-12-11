/* Start Header *****************************************************************/
/*!
\file   PrefabAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for PrefabManager interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for PrefabManager operations.
/// </summary>
internal static partial class PrefabAPI
{
    // ============================================================================
    // PrefabManager Lifecycle
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_GetInstance")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetPrefabManagerInstance();

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_GetFromScene")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetPrefabManagerFromScene(void* scenePtr);

    // ============================================================================
    // Prefab Registration & Hash Lookup
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_RegisterPrefab", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint RegisterPrefab(void* prefabManagerPtr, string path);

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_GetPrefabPath", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial string GetPrefabPath(void* prefabManagerPtr, uint hash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_IsRegistered")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool IsRegistered(void* prefabManagerPtr, uint hash);

    // ============================================================================
    // Prefab Instantiation
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_Instantiate", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint Instantiate(void* prefabManagerPtr, string path);

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_InstantiateAsChild", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint InstantiateAsChild(void* prefabManagerPtr, string path, uint parentEntityId);

    // ============================================================================
    // Instance Tracking
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_TrackInstance")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void TrackInstance(void* prefabManagerPtr, uint entityId, uint prefabHash);

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_UntrackInstance")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void UntrackInstance(void* prefabManagerPtr, uint entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_GetInstanceCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint GetInstanceCount(void* prefabManagerPtr, uint prefabHash);

    // ============================================================================
    // Prefab Synchronization
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_SynchronizeInstance", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool SynchronizeInstance(void* prefabManagerPtr, uint entityId, string prefabPath);

    [LibraryImport("GrapeEngineNative", EntryPoint = "PrefabManagerInterop_DetachInstance")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool DetachInstance(void* prefabManagerPtr, uint entityId);
}
