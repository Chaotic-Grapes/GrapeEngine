/* Start Header *****************************************************************/
/*!
\file   Input.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   28th October 2025
\brief
P/Invoke declarations for the Input API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Input API.
/// </summary>
internal static partial class InputAPI
{
    // Keyboard input
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsKeyPressed")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool IsKeyPressed(int key);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsKeyDown")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool IsKeyDown(int key);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsKeyUp")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool IsKeyUp(int key);

    // Mouse input
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsMousePressed")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool IsMousePressed(int button);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetMouseX")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double GetMouseX();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetMouseY")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double GetMouseY();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetScrollX")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double GetScrollX();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetScrollY")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double GetScrollY();

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsKeyPressed", CallingConvention = CallingConvention.Cdecl)]
    //public static extern bool IsKeyPressed(int key);

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsKeyDown", CallingConvention = CallingConvention.Cdecl)]
    //public static extern bool IsKeyDown(int key);

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsKeyUp", CallingConvention = CallingConvention.Cdecl)]
    //public static extern bool IsKeyUp(int key);

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_IsMousePressed", CallingConvention = CallingConvention.Cdecl)]
    //public static extern bool IsMousePressed(int button);

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetMouseX", CallingConvention = CallingConvention.Cdecl)]
    //public static extern double GetMouseX();

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetMouseY", CallingConvention = CallingConvention.Cdecl)]
    //public static extern double GetMouseY();

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetScrollX", CallingConvention = CallingConvention.Cdecl)]
    //public static extern double GetScrollX();

    //[DllImport("GrapeEngineNative", EntryPoint = "ScriptAPI_GetScrollY", CallingConvention = CallingConvention.Cdecl)]
    //public static extern double GetScrollY();
}
