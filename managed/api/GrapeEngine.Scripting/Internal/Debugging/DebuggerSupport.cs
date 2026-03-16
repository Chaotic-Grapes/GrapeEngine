using System.Diagnostics;

namespace GrapeEngine.Scripting.Internal.Debugging;

/// <summary>
/// Provides debugging support for the managed scripting system.
/// Allows attaching debuggers and accessing diagnostic information.
/// </summary>
public static class DebuggerSupport
{
    private static readonly Lock s_lock = new();

    /// <summary>
    /// Check if a debugger is currently attached.
    /// </summary>
    public static bool IsDebuggerAttached => Debugger.IsAttached;

    /// <summary>
    /// Request to attach a debugger to the current process.
    /// Works on Windows with Visual Studio or WinDbg installed.
    /// </summary>
    public static void AttachDebugger()
    {
        lock (s_lock)
        {
            if (Debugger.IsAttached)
            {
                Logging.LogInternal("[DebuggerSupport] Debugger already attached", LogLevel.Info);
                return;
            }

            try
            {
                Logging.LogInternal("[DebuggerSupport] Attempting to attach debugger...", LogLevel.Info);
                Debugger.Launch();

                if (Debugger.IsAttached)
                {
                    Logging.LogInternal("[DebuggerSupport] Debugger attached successfully", LogLevel.Info);
                }
                else
                {
                    Logging.LogInternal("[DebuggerSupport] Debugger attach was cancelled", LogLevel.Info);
                }
            }
            catch (Exception ex) when (ex is not OutOfMemoryException and not StackOverflowException)
            {
                Logging.LogInternal($"[DebuggerSupport] Failed to attach debugger: {ex.Message}", LogLevel.Error);
            }
        }
    }

    /// <summary>
    /// Break into the debugger if one is attached.
    /// Has no effect if no debugger is attached.
    /// </summary>
    public static void Break()
    {
        if (Debugger.IsAttached)
        {
            Debugger.Break();
        }
    }
}
