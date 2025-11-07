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

namespace GrapeEngine;

public enum LogLevel
{
    Info,
    Debug,
    Warning,
    Error
}

internal static class Logging
{
    internal static void Log(string message, LogLevel level)
    {
        // Change color based on log level:
        // Info     = default
        // Warning  = Yellow
        // Error    = Red
        switch (level)
        {
            case LogLevel.Info:
                Console.WriteLine($"[INF] {message}");
                break;
            case LogLevel.Debug:
                Console.WriteLine($"[DBG] {message}");
                break;
            case LogLevel.Warning:
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"[WRN] {message}");
                Console.ResetColor();
                break;
            case LogLevel.Error:
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"[ERR] {message}");
                Console.ResetColor();
                break;
        }
    }
}
