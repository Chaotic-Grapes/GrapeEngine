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

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for World/Entity operations.
/// 
/// ERROR HANDLING NOTES:
/// - CreateEntity returns entity ID (0 = invalid, but doesn't distinguish error types)
/// - GetComponentPtr returns nullptr when: component doesn't exist OR world is null OR entity is dead
/// - AddComponent returns nullptr on failure (ambiguous error)
/// - RemoveComponent is void (silent failure if world is null)
/// - IsEntityAlive returns false on any error (including null world)
/// 
/// RECOMMENDATION FOR FUTURE IMPROVEMENT:
/// Standardize to return error codes:
///   public static unsafe partial int GetComponentPtr(..., void** outPtr)
///   // Returns: 0 = success, 1 = null world, 2 = dead entity, 3 = component not found
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

    // ============================================================================
    // Hierarchy Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_AttachChild")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void AttachChild(void* worldPtr, ulong childId, ulong parentId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_DetachChild")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void DetachChild(void* worldPtr, ulong childId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_GetParent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetParent(void* worldPtr, ulong childId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_GetFirstChild")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetFirstChild(void* worldPtr, ulong parentId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_GetNextSibling")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetNextSibling(void* worldPtr, ulong childId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_GetChildCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int GetChildCount(void* worldPtr, ulong parentId);

    // ============================================================================
    // Component Serialization Callback Registration
    // ============================================================================
    
    // Managed serializer callback registration (native will call into managed callback)
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_RegisterSerializeCallback")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void RegisterSerializeCallback(nint callbackPtr);

    // Managed deserializer callback registration (native will call into managed callback)
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_RegisterDeserializeCallback")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void RegisterDeserializeCallback(nint callbackPtr);

    // Serialize a component (engine-owned entrypoint) — returns CoTaskMem UTF8 pointer (must be freed)
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_SerializeComponentToJson")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial nint SerializeComponentToJson(void* worldPtr, ulong entityId, uint componentTypeHash);

    // Deserialize a component from JSON (called during play state restoration)
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_DeserializeComponentFromJson", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void DeserializeComponentFromJson(void* worldPtr, ulong entityId, uint componentTypeHash, string jsonStr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_FreeSerializedString")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void FreeSerializedString(nint ptr);
}


