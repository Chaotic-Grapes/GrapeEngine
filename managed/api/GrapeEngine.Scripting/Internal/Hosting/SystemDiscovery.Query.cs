using System;
using System.Collections.Generic;
using System.Linq;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static partial class SystemDiscovery
{
    /// <summary>
    /// Get a previously instantiated system by handle.
    /// </summary>
    public static object? GetSystemInstance(ulong handle)
    {
        lock (_sync)
        {
            return _systemInstances.TryGetValue(handle, out object? instance) ? instance : null;
        }
    }

    /// <summary>
    /// Get the type of a discovered system by handle.
    /// </summary>
    public static Type? GetSystemType(ulong handle)
    {
        lock (_sync)
        {
            return _systemTypes.TryGetValue(handle, out Type? type) ? type : null;
        }
    }

    /// <summary>
    /// Get all currently instantiated system instances.
    /// </summary>
    public static IEnumerable<(ulong Handle, object Instance)> GetAllSystemInstances()
    {
        lock (_sync)
        {
            return _systemInstances.Select(x => (x.Key, x.Value)).ToArray();
        }
    }
}
