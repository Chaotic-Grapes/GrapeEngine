using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static partial class AssemblyManager
{
    /// <summary>
    /// Get a previously loaded assembly by path.
    /// </summary>
    public static Assembly? GetLoadedAssembly(string assemblyPath)
    {
        string trackingKey = NormalizeAssemblyKey(assemblyPath);
        lock (s_sync)
        {
            return s_loadedAssemblies.TryGetValue(trackingKey, out var entry) ? entry.Assembly : null;
        }
    }

    /// <summary>
    /// Get all currently loaded assemblies.
    /// </summary>
    public static IEnumerable<Assembly> GetAllLoadedAssemblies()
    {
        lock (s_sync)
        {
            return s_loadedAssemblies.Values.Select(x => x.Assembly).ToArray();
        }
    }

    /// <summary>
    /// Check if an assembly is currently loaded.
    /// </summary>
    public static bool IsAssemblyLoaded(string assemblyPath)
    {
        string trackingKey = NormalizeAssemblyKey(assemblyPath);
        lock (s_sync)
        {
            return s_loadedAssemblies.ContainsKey(trackingKey);
        }
    }
}
