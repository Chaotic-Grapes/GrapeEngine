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

namespace GrapeEngine.ScriptAPI.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Window API.
/// </summary>
internal partial class WindowAPI
{
    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_GetWidth")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetWidth();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_GetHeight")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetHeight();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_ShouldClose")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool ShouldClose();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_Close")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Close();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_IsFocused")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsFocused();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_IsMinimized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsMinimized();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_SetMinimized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMinimized([MarshalAs(UnmanagedType.I1)] bool minimized);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_IsMaximized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsMaximized();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_SetMaximized")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMaximized([MarshalAs(UnmanagedType.I1)] bool maximized);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_IsVisible")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsVisible();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_SetVisible")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetVisible([MarshalAs(UnmanagedType.I1)] bool visible);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_IsResizable")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsResizable();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_SetResizable")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetResizable([MarshalAs(UnmanagedType.I1)] bool resizable);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_SetMode")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMode(int mode);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_HasMode")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool HasMode(int mode);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Window_Resize")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Resize(int width, int height);
}
