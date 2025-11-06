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
