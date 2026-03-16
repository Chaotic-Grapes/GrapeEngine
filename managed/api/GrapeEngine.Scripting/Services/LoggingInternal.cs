namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Internal logging utility used by systems to log messages at different levels.
/// NOTE: This is internal-only. Scripts should use the public Log API instead.
/// </summary>
internal static class Logging
{
    internal static void LogInternal(string message, LogLevel level)
    {
#if !DEBUG
        return;
#else
        var time = TimeOnly.FromDateTime(DateTime.Now).ToString(@"hh:mm");

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
