using GrapeEngine.Math;
using System.Diagnostics;

namespace GrapeEngine.Scripting.Internal.Debugging;


/// <summary>
/// Diagnostics collector for the scripting system.
/// </summary>
public static class ScriptingDiagnostics
{
    private static readonly Dictionary<ulong, SystemDiagnostics> s_systemDiagnostics = [];
    private static readonly Lock _lockObj = new();

    /// <summary>
    /// Record a system's creation.
    /// </summary>
    public static void RecordSystemCreation(ulong handle, Type systemType, object instance, string name)
    {
        lock (_lockObj)
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
        lock (_lockObj)
        {
            s_systemDiagnostics.Remove(handle);
        }
    }

    /// <summary>
    /// Record an update call for a system.
    /// </summary>
    public static void RecordSystemUpdate(ulong handle, double updateTimeMs)
    {
        lock (_lockObj)
        {
            if (s_systemDiagnostics.TryGetValue(handle, out var diags))
            {
                diags.TotalUpdateCalls++;
                diags.AverageUpdateTimeMs = 
                    (diags.AverageUpdateTimeMs * (diags.TotalUpdateCalls - 1) + updateTimeMs) / 
                    diags.TotalUpdateCalls;
                diags.MaxUpdateTimeMs = GMath.Max(diags.MaxUpdateTimeMs, updateTimeMs);
            }
        }
    }

    /// <summary>
    /// Record an error in a system.
    /// </summary>
    public static void RecordSystemError(ulong handle, string errorMessage)
    {
        lock (_lockObj)
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
        lock (_lockObj)
        {
            return s_systemDiagnostics.TryGetValue(handle, out var diags) ? diags : null;
        }
    }

    /// <summary>
    /// Get diagnostics for all loaded systems.
    /// </summary>
    public static List<SystemDiagnostics> GetAllSystemDiagnostics()
    {
        lock (_lockObj)
        {
            return [..s_systemDiagnostics.Values];
        }
    }

    /// <summary>
    /// Print a diagnostic report for all systems.
    /// </summary>
    public static void PrintDiagnosticReport()
    {
        lock (_lockObj)
        {
            if (s_systemDiagnostics.Count == 0)
            {
                Logging.LogInternal("[ScriptingDiagnostics] No systems loaded", LogLevel.Info);
                return;
            }

            Logging.LogInternal("\n" + new string('=', 120), LogLevel.Info);
            Logging.LogInternal("System Diagnostics Report".PadRight(120), LogLevel.Info);
            Logging.LogInternal(new string('=', 120), LogLevel.Info);
            Logging.LogInternal("System Name".PadRight(40) + "Updates".PadRight(12) + "Avg Time (ms)".PadRight(15) + "Max Time (ms)".PadRight(15) + "Last Error".PadRight(40), LogLevel.Info);
            Logging.LogInternal(new string('-', 120), LogLevel.Info);

            foreach (var diags in s_systemDiagnostics.Values.OrderBy(d => d.SystemName))
            {
                var errorMsg = string.IsNullOrEmpty(diags.LastError) 
                    ? "None" 
                    : diags.LastError[..GMath.Min(30, diags.LastError.Length)];

                Logging.LogInternal(diags.SystemName.PadRight(40) + diags.TotalUpdateCalls.ToString().PadRight(12) + diags.AverageUpdateTimeMs.ToString("F3").PadRight(15) + diags.MaxUpdateTimeMs.ToString("F3").PadRight(15) + errorMsg.PadRight(40), LogLevel.Info);
            }

            Logging.LogInternal(new string('=', 120) + "\n", LogLevel.Info);
        }
    }

    /// <summary>
    /// Get the total number of systems currently loaded.
    /// </summary>
    public static int GetLoadedSystemCount()
    {
        lock (_lockObj)
        {
            return s_systemDiagnostics.Count;
        }
    }

    /// <summary>
    /// Get the total number of update calls across all systems.
    /// </summary>
    public static long GetTotalUpdateCalls()
    {
        lock (_lockObj)
        {
            return s_systemDiagnostics.Values.Sum(d => d.TotalUpdateCalls);
        }
    }

    /// <summary>
    /// Clear all diagnostic data.
    /// </summary>
    public static void Reset()
    {
        lock (_lockObj)
        {
            s_systemDiagnostics.Clear();
        }
    }
}
