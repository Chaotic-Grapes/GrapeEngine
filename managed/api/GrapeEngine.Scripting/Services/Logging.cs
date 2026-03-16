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
Log.Info("Player spawned");
Log.Warn("Low memory");
Log.Error("Failed to load scene");
\endcode

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


using GrapeEngine.Scripting.Internal.Unsafe;
using System.Runtime.CompilerServices;
using System.Text;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Log level for script logging. Ordered by severity for filtering.
/// </summary>
public enum LogLevel : byte
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
}

/// <summary>
/// Public logging API for scripts. Provides static methods to log messages
/// at different severity levels with optional source location tracking.
/// 
/// Usage:
/// <code>
/// Log.Info("Player spawned");
/// Log.Warn("Low memory");
/// Log.Error("Failed to load scene");
/// </code>
/// </summary>
public static class Log
{
    /// <summary>
    /// Log a message at the specified level.
    /// </summary>
    /// <param name="message">Message text</param>
    /// <param name="level">Log level</param>
    public static void Write(
        string message,
        LogLevel level = LogLevel.Info)
    {
#if DEBUG
        DebugAPI.ScriptLog(message, (byte)level);
#endif
    }

    /// <summary>
    /// Log a message using a factory function.
    /// </summary>
    /// <param name="messageFactory">Function that produces the message text</param>
    /// <param name="level">Log level</param>
    public static void Write(
        Func<string> messageFactory,
        LogLevel level = LogLevel.Info)
    {
#if DEBUG
        DebugAPI.ScriptLog(messageFactory(), (byte)level);
#endif
    }

    /// <summary>
    /// Log a message at the specified level.
    /// </summary>
    /// <param name="message">Message text</param>
    /// <param name="level">Log level</param>
    /// <param name="file">Source file path</param>
    /// <param name="line">Source line number</param>
    public static void Write(
        string message,
        LogLevel level = LogLevel.Info,
        string file = "",
        int line = 0)
    {
#if DEBUG
        DebugAPI.ScriptLogWithLocation(message, (byte)level, file, line);
#endif
    }

    /// <summary>
    /// Log a message using a factory function
    /// </summary>
    /// <param name="messageFactory">Function that produces the message text</param>
    /// <param name="level">Log level</param>
    /// <param name="file">Source file path</param>
    /// <param name="line">Source line number</param>
    public static void Write(
        Func<string> messageFactory,
        LogLevel level = LogLevel.Info,
        string file = "",
        int line = 0)
    {
#if DEBUG
        DebugAPI.ScriptLogWithLocation(messageFactory(), (byte)level, file, line);
#endif
    }
}

/// <summary>
/// Internal logging utility used by systems to log messages at different levels.
/// Uses buffering to batch multiple log messages into fewer P/Invoke calls.
/// This significantly reduces the overhead of logging from hot paths.
/// 
/// NOTE: This is internal-only. Scripts should use the public Log API instead.
/// </summary>
internal static class Logging
{
    internal static void LogInternal(string message, LogLevel level)
    {
#if !DEBUG
        return;
#else
        // Log level colors:
        // Debug: Cyan
        // Info: White
        // Warning: Yellow
        // Error: Red
        // Fatal: Red

        // Timestamp in hh:mm format
        var time = TimeOnly.FromDateTime(DateTime.Now)
            .ToString(@"hh:mm");

        switch (level)
        {
            case LogLevel.Info:
                Console.ForegroundColor = ConsoleColor.White;
                Console.WriteLine($"[{time}] [INF C#] {message}");
                break;
            case LogLevel.Debug:
                Console.ForegroundColor = ConsoleColor.Cyan;
                Console.WriteLine($"[{time}] [DBG C#] {message}");
                break;
            case LogLevel.Warning:
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"[{time}] [WRN C#] {message}");
                break;
            case LogLevel.Error:
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"[{time}] [ERR C#] {message}");
                break;
            case LogLevel.Fatal:
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"[{time}] [FTL C#] {message}");
                break;
        }
        Console.ResetColor();
#endif
    }
}
