using System;
using System.Collections.Generic;
using System.Linq;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static partial class SystemDiscovery
{
    /// <summary>
    /// Clear all discovered systems and instances.
    /// Used during assembly reload. Disposes IDisposable instances first.
    /// </summary>
    public static void ClearDiscoveredSystems()
    {
        List<object> instances;
        lock (_sync)
        {
            instances = _systemInstances.Values.ToList();
            _systemTypes.Clear();
            _handlesByType.Clear();
            _systemInstances.Clear();
            _nextSystemHandle = 1;
        }

        // Try to dispose instances that implement IDisposable
        foreach (object instance in instances)
        {
            try
            {
                if (instance is IDisposable disposable)
                {
                    disposable.Dispose();
                }
            }
            catch (Exception ex) when (IsRecoverableSystemException(ex))
            {
                Logging.LogInternal($"[SystemDiscovery] Error disposing system instance: {ex.Message}", LogLevel.Warning);
            }
        }

        Logging.LogInternal("[SystemDiscovery] Cleared all discovered systems and instances", LogLevel.Info);
    }
}
