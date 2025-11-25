/* Start Header *****************************************************************/
/*!
\file   UIAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   25th November 2025
\brief
P/Invoke declarations for the UI API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the UI API.
/// </summary>
internal static partial class UIAPI
{
    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_UI_WasAnyClicked")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool WasAnyClicked();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_UI_GetClickedActionID")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial uint GetClickedActionID();
}
