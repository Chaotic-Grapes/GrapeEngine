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


using GrapeEngine.Scripting.Internal.Unsafe;
using System.Text;

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
/// Uses buffering to batch multiple log messages into fewer P/Invoke calls.
/// This significantly reduces the overhead of logging from hot paths.
/// </summary>
internal static class Logging
{
    private static readonly StringBuilder _logBuffer = new();
    private static LogLevel _lastLogLevel = LogLevel.Info;
    private const int BufferSizeThreshold = 2048; // Flush when buffer exceeds 2KB

    /// <summary>
    /// Log a message at the specified level using the debug API.
    /// Messages are buffered and sent in batches to reduce P/Invoke overhead.
    /// </summary>
    internal static void Log(string message, LogLevel level)
    {
        // If log level changes, flush existing buffer with previous level
        if (level != _lastLogLevel && _logBuffer.Length > 0)
        {
            FlushBuffer(_lastLogLevel);
        }

        _lastLogLevel = level;
        
        // Append to buffer with newline
        _logBuffer.AppendLine(message);

        // Flush if buffer gets too large to prevent excessive memory usage
        if (_logBuffer.Length >= BufferSizeThreshold)
        {
            FlushBuffer(level);
        }
    }

    /// <summary>
    /// Immediately flush any buffered logs to the native side.
    /// Call this at the end of each frame to ensure logs are delivered.
    /// </summary>
    internal static void Flush()
    {
        if (_logBuffer.Length > 0)
        {
            FlushBuffer(_lastLogLevel);
        }
    }

    private static void FlushBuffer(LogLevel level)
    {
        if (_logBuffer.Length == 0)
            return;

        var message = _logBuffer.ToString();
        _logBuffer.Clear();

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

    internal static void LogInternal(string message, LogLevel level)
    {
        // Log level colors:
        // Info: White
        // Debug: Cyan
        // Warning: Yellow
        // Error: Red

        // Timestamp in hh:mm format
        var time = TimeOnly.FromDateTime(DateTime.Now)
            .ToString(@"hh:mm");

        switch (level)
        {
            case LogLevel.Info:
                Console.ForegroundColor = ConsoleColor.White;
                Console.WriteLine($"[{time}][INF C#] {message}");
                break;
            case LogLevel.Debug:
                Console.ForegroundColor = ConsoleColor.Cyan;
                Console.WriteLine($"[{time}][DBG C#] {message}");
                break;
            case LogLevel.Warning:
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"[{time}][WRN C#] {message}");
                break;
            case LogLevel.Error:
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"[{time}][ERR C#] {message}");
                break;
        }
        Console.ResetColor();
    }
}

