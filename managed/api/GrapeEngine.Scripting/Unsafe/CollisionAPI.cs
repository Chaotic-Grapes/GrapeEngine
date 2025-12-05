/* Start Header *****************************************************************/
/*!
\file   CollisionAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for Collision Events interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for Collision Events operations.
/// </summary>
internal static partial class CollisionAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_Clear")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Clear(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetEventCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint GetEventCount(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool GetEvent(void* worldPtr, ulong entityId, uint index, ulong* outOtherEntityId, int* outEventType);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetEventsBulk")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool GetEventsBulk(void* worldPtr, ulong entityId, ulong* outOtherEntityIds, int* outEventTypes, uint maxCount, uint* outActualCount);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_HasCollisionWith")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool HasCollisionWith(void* worldPtr, ulong entityId, ulong otherEntityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Collision_GetCollisionType")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int GetCollisionType(void* worldPtr, ulong entityId, ulong otherEntityId);
}
