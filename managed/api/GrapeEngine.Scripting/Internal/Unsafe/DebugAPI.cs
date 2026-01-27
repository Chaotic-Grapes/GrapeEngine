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

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Debug API.
/// </summary>
internal partial class DebugAPI
{
    /// <summary>
    /// Report a single compiler diagnostic with structured data.
    /// One call per diagnostic for O(1) processing per diagnostic.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_ScriptDiagnostic", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ScriptDiagnostic(string id, byte severity, string file, int line, int column, string message);

    /// <summary>
    /// Log a message from script code at runtime.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_ScriptLog", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ScriptLog(string message, byte level);

    /// <summary>
    /// Log a message from script code with source location information.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_ScriptLogWithLocation", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void ScriptLogWithLocation(string message, byte level, string file, int line);
}


