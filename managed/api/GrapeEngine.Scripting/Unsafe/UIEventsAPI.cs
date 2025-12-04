/* Start Header *****************************************************************/
/*!
\file   UIEventsAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th November 2025
\brief
P/Invoke declarations for the UI Events API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the UI Events API.
/// </summary>
internal partial class UIEventsAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_Clear")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Clear();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_WasClicked")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool WasClicked(ulong entityId, int button);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_WasHoverEntered")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool WasHoverEntered(ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_WasHoverExited")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool WasHoverExited(ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_GetEventCount")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetEventCount(ulong entityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_GetEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static unsafe partial bool GetEvent(ulong entityId, int eventIndex,
        int* outType, int* outButton, float* outScreenX, float* outScreenY);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_GetHoveredEntity")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong GetHoveredEntity();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_UIEvents_IsMouseOverUI")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsMouseOverUI();
}
