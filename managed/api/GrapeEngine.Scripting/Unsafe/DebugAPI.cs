/* Start Header *****************************************************************/
/*!
\file   DebugAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
P/Invoke declarations for the Debug API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Debug API.
/// </summary>
internal partial class DebugAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Debug_LogInfo", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void LogInfo(string message);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Debug_LogDebug", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void LogDebug(string message);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Debug_LogWarning", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void LogWarning(string message);

    [LibraryImport("GrapeEngineNative", EntryPoint = "ScriptAPI_Debug_LogError", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void LogError(string message);
}
