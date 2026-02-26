/* Start Header *****************************************************************/
/*!
\file   WindowAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
P/Invoke declarations for the Window API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Window API.
/// </summary>
internal partial class WindowAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_GetWidth")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetWidth();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_GetHeight")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetHeight();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_ShouldClose")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool ShouldClose();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_Close")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Close();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_IsFocused")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsFocused();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_IsMinimized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsMinimized();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetMinimized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMinimized([MarshalAs(UnmanagedType.I1)] bool minimized);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_IsMaximized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsMaximized();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetMaximized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMaximized([MarshalAs(UnmanagedType.I1)] bool maximized);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_IsVisible")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsVisible();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetVisible")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetVisible([MarshalAs(UnmanagedType.I1)] bool visible);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_IsResizable")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsResizable();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetResizable")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetResizable([MarshalAs(UnmanagedType.I1)] bool resizable);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetMode")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMode(int mode);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_HasMode")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool HasMode(int mode);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_Resize")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Resize(int width, int height);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_GetTitle")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial IntPtr GetTitle();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetTitle", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetTitle(string title);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_IsVSync")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsVSync();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetVSync")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetVSync([MarshalAs(UnmanagedType.I1)] bool enabled);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetFullscreen")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SetFullscreen([MarshalAs(UnmanagedType.I1)] bool fullscreen);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetFullscreenOnMonitor")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SetFullscreenOnMonitor(int monitorIndex);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_GetSupportedDisplayModeCount")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetSupportedDisplayModeCount();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_GetSupportedDisplayMode")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool GetSupportedDisplayMode(int index, out int width, out int height,
        out int refreshRate, out int bitsPerPixel);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Window_SetDisplayMode")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SetDisplayMode(int width, int height, int refreshRate, int bitsPerPixel);
}


