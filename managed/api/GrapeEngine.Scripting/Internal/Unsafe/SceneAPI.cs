/* Start Header *****************************************************************/
/*!
\file   SceneAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for Scene/SceneManager interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for Scene/SceneManager operations.
/// </summary>
internal static partial class SceneAPI
{
    // ============================================================================
    // SceneManager Lifecycle
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_GetInstance")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetSceneManagerInstance();

    // ============================================================================
    // Scene Management - Add/Remove/Query
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_AddScene")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong AddScene(void* sceneManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_RemoveScene")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool RemoveScene(void* sceneManagerPtr, ulong sceneIndex);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_GetSceneCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetSceneCount(void* sceneManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_GetScene")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetScene(void* sceneManagerPtr, ulong sceneIndex);

    // ============================================================================
    // Scene Activation
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_SetActive")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetActive(void* sceneManagerPtr, ulong sceneIndex);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_SetActiveImmediate")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetActiveImmediate(void* sceneManagerPtr, ulong sceneIndex);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_GetActive")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetActive(void* sceneManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_GetActiveIndex")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetActiveIndex(void* sceneManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_GetPendingIndex")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetPendingIndex(void* sceneManagerPtr);

    // ============================================================================
    // Scene Update
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_Update")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Update(void* sceneManagerPtr);

    // ============================================================================
    // Scene Serialization
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_SaveScene", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool SaveScene(void* sceneManagerPtr, ulong sceneIndex, string filename, string sceneName, string version);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneManagerInterop_LoadScene", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool LoadScene(void* sceneManagerPtr, ulong sceneIndex, string filename);

    // ============================================================================
    // Scene Properties
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_GetName")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void GetName(void* scenePtr, byte* buffer, int bufferSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_SetName", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetName(void* scenePtr, string name);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_GetPath")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void GetPath(void* scenePtr, byte* buffer, int bufferSize);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_SetPath", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetPath(void* scenePtr, string path);

    // ============================================================================
    // Scene World Access
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_GetWorld")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetWorld(void* scenePtr);

    // ============================================================================
    // Scene Entity Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_CreateEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong CreateEntity(void* scenePtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_CreateEntityWithParent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong CreateEntityWithParent(void* scenePtr, ulong parentId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_DestroyEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void DestroyEntity(void* scenePtr, ulong entityId);

    // ============================================================================
    // Prefab Management
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "SceneInterop_GetPrefabManager")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* GetPrefabManager(void* scenePtr);
}


