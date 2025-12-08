/* Start Header *****************************************************************/
/*!
\file   DebuggerSupport.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Debugger support and diagnostics for managed scripting system.
Enables attaching debuggers and logging diagnostic information.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Diagnostics;

namespace GrapeEngine.Scripting.Debugging;

/// <summary>
/// Provides debugging support for the managed scripting system.
/// Allows attaching debuggers and accessing diagnostic information.
/// </summary>
public static class DebuggerSupport
{
    private static bool s_debuggerAttached = false;
    private static readonly object s_lockObj = new();

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
        lock (s_lockObj)
        {
            if (Debugger.IsAttached)
            {
                Console.WriteLine("[DebuggerSupport] Debugger already attached");
                s_debuggerAttached = true;
                return;
            }

            try
            {
                Console.WriteLine("[DebuggerSupport] Attempting to attach debugger...");
                Debugger.Launch();
                s_debuggerAttached = true;
                
                if (Debugger.IsAttached)
                {
                    Console.WriteLine("[DebuggerSupport] Debugger attached successfully");
                }
                else
                {
                    Console.WriteLine("[DebuggerSupport] Debugger attach was cancelled");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[DebuggerSupport] Failed to attach debugger: {ex.Message}");
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

    /// <summary>
    /// Log a debug message (writes to debug output).
    /// </summary>
    public static void Log(string message)
    {
        Debug.WriteLine($"[ScriptDebug] {message}");
    }

    /// <summary>
    /// Log a warning message.
    /// </summary>
    public static void LogWarning(string message)
    {
        Debug.WriteLine($"[ScriptWarning] {message}");
    }

    /// <summary>
    /// Log an error message.
    /// </summary>
    public static void LogError(string message)
    {
        Debug.WriteLine($"[ScriptError] {message}");
        if (Debugger.IsAttached)
        {
            Debugger.Break();
        }
    }
}

/// <summary>
/// System diagnostics collector for debugging and profiling.
/// </summary>
public class SystemDiagnostics
{
    public string SystemName { get; set; } = "";
    public Type? SystemType { get; set; }
    public object? Instance { get; set; }
    public bool IsLoaded { get; set; }
    public DateTime LoadedAt { get; set; }
    public long TotalUpdateCalls { get; set; }
    public double AverageUpdateTimeMs { get; set; }
    public double MaxUpdateTimeMs { get; set; }
    public string? LastError { get; set; }
    public DateTime? LastErrorTime { get; set; }
}

/// <summary>
/// Diagnostics collector for the scripting system.
/// </summary>
public static class ScriptingDiagnostics
{
    private static readonly Dictionary<ulong, SystemDiagnostics> s_systemDiagnostics = [];
    private static readonly object s_lockObj = new();

    /// <summary>
    /// Record a system's creation.
    /// </summary>
    public static void RecordSystemCreation(ulong handle, Type systemType, object instance, string name)
    {
        lock (s_lockObj)
        {
            var diags = new SystemDiagnostics
            {
                SystemName = name,
                SystemType = systemType,
                Instance = instance,
                IsLoaded = true,
                LoadedAt = DateTime.UtcNow,
            };
            s_systemDiagnostics[handle] = diags;
        }
    }

    /// <summary>
    /// Record a system's destruction.
    /// </summary>
    public static void RecordSystemDestruction(ulong handle)
    {
        lock (s_lockObj)
        {
            s_systemDiagnostics.Remove(handle);
        }
    }

    /// <summary>
    /// Record an update call for a system.
    /// </summary>
    public static void RecordSystemUpdate(ulong handle, double updateTimeMs)
    {
        lock (s_lockObj)
        {
            if (s_systemDiagnostics.TryGetValue(handle, out var diags))
            {
                diags.TotalUpdateCalls++;
                diags.AverageUpdateTimeMs = 
                    (diags.AverageUpdateTimeMs * (diags.TotalUpdateCalls - 1) + updateTimeMs) / 
                    diags.TotalUpdateCalls;
                diags.MaxUpdateTimeMs = Math.Max(diags.MaxUpdateTimeMs, updateTimeMs);
            }
        }
    }

    /// <summary>
    /// Record an error in a system.
    /// </summary>
    public static void RecordSystemError(ulong handle, string errorMessage)
    {
        lock (s_lockObj)
        {
            if (s_systemDiagnostics.TryGetValue(handle, out var diags))
            {
                diags.LastError = errorMessage;
                diags.LastErrorTime = DateTime.UtcNow;
            }
        }
    }

    /// <summary>
    /// Get diagnostics for a specific system.
    /// </summary>
    public static SystemDiagnostics? GetSystemDiagnostics(ulong handle)
    {
        lock (s_lockObj)
        {
            return s_systemDiagnostics.TryGetValue(handle, out var diags) ? diags : null;
        }
    }

    /// <summary>
    /// Get diagnostics for all loaded systems.
    /// </summary>
    public static List<SystemDiagnostics> GetAllSystemDiagnostics()
    {
        lock (s_lockObj)
        {
            return [..s_systemDiagnostics.Values];
        }
    }

    /// <summary>
    /// Print a diagnostic report for all systems.
    /// </summary>
    public static void PrintDiagnosticReport()
    {
        lock (s_lockObj)
        {
            if (s_systemDiagnostics.Count == 0)
            {
                Console.WriteLine("[ScriptingDiagnostics] No systems loaded");
                return;
            }

            Console.WriteLine("\n" + new string('=', 120));
            Console.WriteLine("System Diagnostics Report".PadRight(120));
            Console.WriteLine(new string('=', 120));
            Console.WriteLine(
                "System Name".PadRight(40) +
                "Updates".PadRight(12) +
                "Avg Time (ms)".PadRight(15) +
                "Max Time (ms)".PadRight(15) +
                "Last Error".PadRight(40)
            );
            Console.WriteLine(new string('-', 120));

            foreach (var diags in s_systemDiagnostics.Values.OrderBy(d => d.SystemName))
            {
                var errorMsg = string.IsNullOrEmpty(diags.LastError) 
                    ? "None" 
                    : diags.LastError.Substring(0, Math.Min(30, diags.LastError.Length));

                Console.WriteLine(
                    diags.SystemName.PadRight(40) +
                    diags.TotalUpdateCalls.ToString().PadRight(12) +
                    diags.AverageUpdateTimeMs.ToString("F3").PadRight(15) +
                    diags.MaxUpdateTimeMs.ToString("F3").PadRight(15) +
                    errorMsg.PadRight(40)
                );
            }

            Console.WriteLine(new string('=', 120) + "\n");
        }
    }

    /// <summary>
    /// Get the total number of systems currently loaded.
    /// </summary>
    public static int GetLoadedSystemCount()
    {
        lock (s_lockObj)
        {
            return s_systemDiagnostics.Count;
        }
    }

    /// <summary>
    /// Get the total number of update calls across all systems.
    /// </summary>
    public static long GetTotalUpdateCalls()
    {
        lock (s_lockObj)
        {
            return s_systemDiagnostics.Values.Sum(d => d.TotalUpdateCalls);
        }
    }

    /// <summary>
    /// Clear all diagnostic data.
    /// </summary>
    public static void Reset()
    {
        lock (s_lockObj)
        {
            s_systemDiagnostics.Clear();
        }
    }
}

/// <summary>
/// Configuration for debugging features.
/// </summary>
public static class DebugConfiguration
{
    /// <summary>
    /// Enable verbose logging of script system operations.
    /// </summary>
    public static bool VerboseLogging { get; set; } = false;

    /// <summary>
    /// Enable automatic diagnostics collection.
    /// </summary>
    public static bool EnableDiagnostics { get; set; } = true;

    /// <summary>
    /// Break into debugger on script system errors.
    /// </summary>
    public static bool BreakOnError { get; set; } = false;

    /// <summary>
    /// Enable stack trace capture on errors.
    /// </summary>
    public static bool CaptureStackTraces { get; set; } = true;

    /// <summary>
    /// Maximum stack trace depth to capture.
    /// </summary>
    public static int MaxStackTraceDepth { get; set; } = 20;
}
