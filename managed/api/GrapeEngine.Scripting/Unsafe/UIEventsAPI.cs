/* Start Header *****************************************************************/
/*!
\file   UIEventsAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for UI Events interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for UI Events operations.
/// </summary>
internal static partial class UIEventsAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_Clear")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void Clear(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_WasClicked")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool WasClicked(void* worldPtr, ulong entityId, int button);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_WasHoverEntered")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool WasHoverEntered(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_WasHoverExited")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool WasHoverExited(void* worldPtr, ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_GetEventCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial int GetEventCount(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_GetEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void GetEvent(void* worldPtr, int index, ulong* outEntityId, int* outEventType);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_GetHoveredEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong GetHoveredEntity(void* worldPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_UIEvents_IsMouseOverUI")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool IsMouseOverUI(void* worldPtr);
}
