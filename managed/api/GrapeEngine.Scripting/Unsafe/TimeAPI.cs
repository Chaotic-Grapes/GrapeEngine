/* Start Header *****************************************************************/
/*!
\file   TimeAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   27th October 2025
\brief
P/Invoke declarations for the Time API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Time API.
/// </summary>
internal partial class TimeAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetDeltaTime();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetUnscaledDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetUnscaledDeltaTime();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetFixedDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetFixedDeltaTime();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetUnscaledFixedDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetUnscaledFixedDeltaTime();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetElapsedTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double GetElapsedTime();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetFrameCount")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetFrameCount();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetTimeScale")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetTimeScale();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_SetTimeScale")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetTimeScale(float scale);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_GetMaximumDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetMaximumDeltaTime();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Time_SetMaximumDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMaximumDeltaTime(float maxDelta);
}
