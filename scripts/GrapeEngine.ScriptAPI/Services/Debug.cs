/* Start Header *****************************************************************/
/*!
\file   Debug.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
Provides debugging and logging functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine;

/// <summary>
/// Provides debugging and logging functionality.
/// </summary>
public static class Debug
{
    /// <summary>
    /// Log a message to the console.
    /// </summary>
    public static void Log(string message) => DebugAPI.Log(message);

    /// <summary>
    /// Log a warning message to the console.
    /// </summary>
    public static void LogWarning(string message) => DebugAPI.LogWarning(message);

    /// <summary>
    /// Log an error message to the console.
    /// </summary>
    public static void LogError(string message) => DebugAPI.LogError(message);
}
