namespace GrapeEngine.Scripting
{
    public enum LogLevel
    {
        Info,
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
                    Console.WriteLine($"[ScriptHost] {message}");
                    break;
                case LogLevel.Warning:
                    Console.ForegroundColor = ConsoleColor.Yellow;
                    Console.WriteLine($"[ScriptHost] WARNING: {message}");
                    Console.ResetColor();
                    break;
                case LogLevel.Error:
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"[ScriptHost] ERROR: {message}");
                    Console.ResetColor();
                    break;
            }
        }
    }
}
