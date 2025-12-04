/* Start Header *****************************************************************/
/*!
\file   PhysicsAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
P/Invoke declarations for the Physics API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.ScriptAPI.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Physics API.
/// </summary>
internal partial class PhysicsAPI
{
    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_SetGravity")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetGravity(float x, float y);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_GetGravity")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void GetGravity(out float x, out float y);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_SetEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetEnabled([MarshalAs(UnmanagedType.I1)] bool enabled);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_IsEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsEnabled();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_ApplyForce")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ApplyForce(ulong entityId, float forceX, float forceY);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_ApplyImpulse")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ApplyImpulse(ulong entityId, float impulseX, float impulseY);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_GetVelocity")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void GetVelocity(ulong entityId, out float x, out float y);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Physics_SetVelocity")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetVelocity(ulong entityId, float x, float y);
}
