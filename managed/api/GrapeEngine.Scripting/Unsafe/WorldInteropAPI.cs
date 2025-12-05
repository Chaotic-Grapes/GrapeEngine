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

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for World/Entity operations.
/// </summary>
internal static partial class WorldInteropAPI
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
    // Physics Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_SetGravity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_SetGravity(void* worldPtr, float x, float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_GetGravity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_GetGravity(void* worldPtr, float* outX, float* outY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_SetEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_SetEnabled(void* worldPtr, [MarshalAs(UnmanagedType.Bool)] bool enabled);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_IsEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool Physics_IsEnabled(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_ApplyForce")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_ApplyForce(void* worldPtr, ulong entityId, float forceX, float forceY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_ApplyImpulse")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_ApplyImpulse(void* worldPtr, ulong entityId, float impulseX, float impulseY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_GetVelocity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_GetVelocity(void* worldPtr, ulong entityId, float* outX, float* outY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_SetVelocity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Physics_SetVelocity(void* worldPtr, ulong entityId, float x, float y);

    // ============================================================================
    // UI Events
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_Clear")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void UIEvents_Clear(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_WasClicked")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool UIEvents_WasClicked(void* worldPtr, ulong entityId, int button);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_WasHoverEntered")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool UIEvents_WasHoverEntered(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_WasHoverExited")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool UIEvents_WasHoverExited(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_GetEventCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int UIEvents_GetEventCount(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_GetEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void UIEvents_GetEvent(void* worldPtr, int index, ulong* outEntityId, int* outEventType);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_GetHoveredEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong UIEvents_GetHoveredEntity(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_IsMouseOverUI")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool UIEvents_IsMouseOverUI(void* worldPtr);

    // ============================================================================
    // Collision Events
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_Clear")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Collision_Clear(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetEventCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint Collision_GetEventCount(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool Collision_GetEvent(void* worldPtr, ulong entityId, uint index, ulong* outOtherEntityId, int* outEventType);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetEventsBulk")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool Collision_GetEventsBulk(void* worldPtr, ulong entityId, ulong* outOtherEntityIds, int* outEventTypes, uint maxCount, uint* outActualCount);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_HasCollisionWith")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool Collision_HasCollisionWith(void* worldPtr, ulong entityId, ulong otherEntityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetCollisionType")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int Collision_GetCollisionType(void* worldPtr, ulong entityId, ulong otherEntityId);
}
