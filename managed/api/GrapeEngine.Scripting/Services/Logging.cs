/* Start Header *****************************************************************/
/*!
\file   Logging.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th October 2025
\brief
Logging utility for the GrapeEngine scripting API. Provides methods to log messages
at different log levels.

\code
Logging.Log("This is an info message.", LogLevel.Info);
Logging.Log("This is a warning message.", LogLevel.Warning);
Logging.Log("This is an error message.", LogLevel.Error);
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Services;

public enum LogLevel
{
    Info,
    Debug,
    Warning,
    Error
}

/// <summary>
/// Internal logging utility used by systems to log messages at different levels.
/// </summary>
internal static class Logging
{
    /// <summary>
    /// Log a message at the specified level using the debug API.
    /// </summary>
    internal static void Log(string message, LogLevel level)
    {
        switch (level)
        {
            case LogLevel.Info:
                DebugAPI.LogInfo(message);
                break;
            case LogLevel.Debug:
                DebugAPI.LogDebug(message);
                break;
            case LogLevel.Warning:
                DebugAPI.LogWarning(message);
                break;
            case LogLevel.Error:
                DebugAPI.LogError(message);
                break;
        }
    }
}
