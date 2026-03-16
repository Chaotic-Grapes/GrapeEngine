using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Internal logging utility used by systems to log messages at different levels.
/// NOTE: This is internal-only. Scripts should use the public Log API instead.
/// </summary>
internal static class Logging
{
    private static bool IsRecoverableLoggingException(Exception ex)
    {
        return ex is not OutOfMemoryException and not StackOverflowException;
    }

    private static byte ToNativeLogLevel(LogLevel level)
    {
        return level switch
        {
            LogLevel.Debug => 0,
            LogLevel.Info => 1,
            LogLevel.Warning => 2,
            LogLevel.Error => 3,
            LogLevel.Fatal => 4,
            _ => 1
        };
    }

    private static bool ShouldMirrorToConsolePanel(LogLevel level, bool logToConsolePanel)
    {
        if (logToConsolePanel)
        {
            return true;
        }

        return level is LogLevel.Warning or LogLevel.Error or LogLevel.Fatal;
    }

    private static string SanitizeForConsolePanel(string message)
    {
        if (string.IsNullOrWhiteSpace(message))
        {
            return message;
        }

        int index = 0;
        while (index < message.Length)
        {
            while (index < message.Length && char.IsWhiteSpace(message[index]))
            {
                index++;
            }

            if (index >= message.Length || message[index] != '[')
            {
                break;
            }

            int closing = message.IndexOf(']', index + 1);
            if (closing < 0)
            {
                break;
            }

            index = closing + 1;
        }

        if (index <= 0 || index >= message.Length)
        {
            return message.Trim();
        }

        return message[index..].TrimStart();
    }

    internal static void LogInternal(string message, LogLevel level, bool logToConsolePanel = false)
    {
#if GAME_EXPORT
        return;
#else
        // Keep stdout logging regardless of native logging availability.
        var time = TimeOnly.FromDateTime(DateTime.Now).ToString(@"hh:mm");
        string prefix = level switch
        {
            LogLevel.Debug => "DBG",
            LogLevel.Info => "INF",
            LogLevel.Warning => "WRN",
            LogLevel.Error => "ERR",
            LogLevel.Fatal => "FTL",
            _ => "INF"
        };
        Console.WriteLine($"[{time}] [{prefix} C#] {message}");

        if (!ShouldMirrorToConsolePanel(level, logToConsolePanel))
        {
            return;
        }

        try
        {
            DebugAPI.ScriptLog(SanitizeForConsolePanel(message), ToNativeLogLevel(level));
        }
        catch (Exception ex) when (IsRecoverableLoggingException(ex))
        {
        }
#endif
    }
}
