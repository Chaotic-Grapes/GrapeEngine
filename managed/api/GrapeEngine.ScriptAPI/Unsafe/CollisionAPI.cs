/* Start Header *****************************************************************/
/*!
\file   CollisionAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025
\brief
P/Invoke declarations for the Collision API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.ScriptAPI.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for collision event access.
/// </summary>
internal partial class CollisionAPI
{
    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Collision_GetEventCount")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint GetEventCount(ulong entityId);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Collision_GetEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool GetEvent(ulong entityId, uint index, out ulong outOtherEntity, out int outEventType);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Collision_GetEventsBulk")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint GetEventsBulk(ulong entityId,
        [Out] ulong[] outOtherEntities, [Out] int[] outEventTypes, uint capacity);
}
