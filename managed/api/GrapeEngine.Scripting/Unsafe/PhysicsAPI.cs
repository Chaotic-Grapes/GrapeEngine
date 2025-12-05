/* Start Header *****************************************************************/
/*!
\file   PhysicsAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for Physics interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for Physics operations.
/// </summary>
internal static partial class PhysicsAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_SetGravity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetGravity(void* worldPtr, float x, float y);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_GetGravity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void GetGravity(void* worldPtr, float* outX, float* outY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_SetEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetEnabled(void* worldPtr, [MarshalAs(UnmanagedType.Bool)] bool enabled);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_IsEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool IsEnabled(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_ApplyForce")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void ApplyForce(void* worldPtr, ulong entityId, float forceX, float forceY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_ApplyImpulse")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void ApplyImpulse(void* worldPtr, ulong entityId, float impulseX, float impulseY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_GetVelocity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void GetVelocity(void* worldPtr, ulong entityId, float* outX, float* outY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_Physics_SetVelocity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void SetVelocity(void* worldPtr, ulong entityId, float x, float y);
}
