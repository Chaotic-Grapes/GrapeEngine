/* Start Header *****************************************************************/
/*!
\file   SystemDiscovery.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Discovers and manages ISystem implementations via reflection.
*/
/* End Header *******************************************************************/

using System.Reflection;
using GrapeEngine.Scripting.Systems;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// SystemDiscovery - Encapsulates system discovery and instantiation logic.
/// 
/// Responsibilities:
/// - Discover ISystem implementations in loaded assemblies using reflection
/// - Instantiate system objects and track them
/// - Map system types to handles for C++ interop
/// - Manage system lifecycle and hot reload state
/// </summary>
internal static class SystemDiscovery
{
    /// <summary>
    /// Discovered system types, mapped to unique handles.
    /// Key: Handle (opaque identifier for C++)
    /// Value: Type that implements ISystem
    /// </summary>
    private static readonly Dictionary<ulong, Type> _systemTypes = [];
    private static readonly Dictionary<Type, ulong> _handlesByType = [];

    /// <summary>
    /// Instantiated system objects.
    /// Key: Handle
    /// Value: System instance
    /// </summary>
    private static readonly Dictionary<ulong, object> _systemInstances = [];
    private static readonly object _sync = new();

    /// <summary>
    /// Next available system handle.
    /// </summary>
    private static ulong _nextSystemHandle = 1;

    /// <summary>
    /// Discover all ISystem implementations in the given assembly.
    /// 
    /// Uses reflection to find all public types that inherit from ISystem.
    /// Each discovered system is assigned a unique handle.
    /// </summary>
    /// <param name="assembly">Assembly to search for systems</param>
    /// <returns>Array of discovered system types (full names)</returns>
    public static string[] DiscoverSystemsInAssembly(Assembly assembly)
    {
        try
        {
            List<Type> systemTypes = assembly
                .GetTypes()
                .Where(t => typeof(ISystem).IsAssignableFrom(t) && t is { IsInterface: false, IsAbstract: false })
                .ToList();

            var result = new List<string>();

            foreach (var systemType in systemTypes)
            {
                ulong handle;
                lock (_sync)
                {
                    if (_handlesByType.TryGetValue(systemType, out ulong existingHandle))
                    {
                        handle = existingHandle;
                    }
                    else
                    {
                        handle = _nextSystemHandle++;
                        _systemTypes[handle] = systemType;
                        _handlesByType[systemType] = handle;
                    }
                }

                result.Add($"{handle}:{systemType.FullName}");
                Logging.LogInternal($"[SystemDiscovery] Discovered system: {systemType.FullName} (handle={handle})", LogLevel.Info);
            }

            return [.. result];
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[SystemDiscovery] Error discovering systems: {ex.Message}", LogLevel.Error);
            return [];
        }
    }

    /// <summary>
    /// Create an instance of a system by handle.
    /// 
    /// Instantiates the system type using parameterless constructor.
    /// Tracks the instance for later access.
    /// </summary>
    /// <param name="handle">System handle returned from discovery</param>
    /// <returns>New system instance, or null if creation failed</returns>
    public static object? CreateSystemInstance(ulong handle)
    {
        try
        {
            lock (_sync)
            {
                if (_systemInstances.TryGetValue(handle, out object? existingInstance))
                {
                    return existingInstance;
                }

                if (!_systemTypes.TryGetValue(handle, out Type? systemType))
                {
                    Logging.LogInternal($"[SystemDiscovery] System not found: handle={handle}", LogLevel.Warning);
                    return null;
                }

                object? instance = Activator.CreateInstance(systemType);
                if (instance == null)
                {
                    Logging.LogInternal($"[SystemDiscovery] Failed to create system instance: {systemType.Name}", LogLevel.Warning);
                    return null;
                }

                _systemInstances[handle] = instance;

                Logging.LogInternal($"[SystemDiscovery] Created system instance: {systemType.Name} (handle={handle})", LogLevel.Info);
                return instance;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[SystemDiscovery] Error creating system instance: {ex.Message}", LogLevel.Error);
            return null;
        }
    }

    /// <summary>
    /// Create a system instance from a specific Type and register it.
    /// Returns the handle assigned to the new instance.
    /// </summary>
    public static ulong CreateSystemInstanceFromType(Type systemType)
    {
        try
        {
            if (!typeof(ISystem).IsAssignableFrom(systemType))
            {
                Logging.LogInternal($"[SystemDiscovery] Type does not implement ISystem: {systemType.Name}", LogLevel.Warning);
                return 0;
            }

            lock (_sync)
            {
                if (_handlesByType.TryGetValue(systemType, out ulong existingHandle))
                {
                    if (_systemInstances.ContainsKey(existingHandle))
                    {
                        return existingHandle;
                    }

                    object? recoveredInstance = Activator.CreateInstance(systemType);
                    if (recoveredInstance == null)
                    {
                        Logging.LogInternal($"[SystemDiscovery] Failed to instantiate: {systemType.Name}", LogLevel.Warning);
                        return 0;
                    }

                    _systemInstances[existingHandle] = recoveredInstance;
                    return existingHandle;
                }

                ulong handle = _nextSystemHandle++;
                _systemTypes[handle] = systemType;
                _handlesByType[systemType] = handle;

                object? instance = Activator.CreateInstance(systemType);
                if (instance == null)
                {
                    _systemTypes.Remove(handle);
                    _handlesByType.Remove(systemType);
                    Logging.LogInternal($"[SystemDiscovery] Failed to instantiate: {systemType.Name}", LogLevel.Warning);
                    return 0;
                }

                _systemInstances[handle] = instance;
                Logging.LogInternal($"[SystemDiscovery] Created system instance: {systemType.Name} (handle={handle})", LogLevel.Info);
                return handle;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[SystemDiscovery] Error creating system from type: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }

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
            catch (Exception ex)
            {
                Logging.LogInternal($"[SystemDiscovery] Error disposing system instance: {ex.Message}", LogLevel.Warning);
            }
        }
        
        Logging.LogInternal($"[SystemDiscovery] Cleared all discovered systems and instances", LogLevel.Info);
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

    /// <summary>
    /// Register an existing ISystem instance and return its assigned handle.
    /// This is useful for systems created by code (not via Activator) or for
    /// registering `SystemBase` instances directly.
    /// </summary>
    public static ulong RegisterInstance(object instance)
    {
        try
        {
            if (instance == null)
                return 0;

            if (instance is not ISystem)
            {
                Logging.LogInternal($"[SystemDiscovery] Instance does not implement ISystem: {instance.GetType().Name}", LogLevel.Warning);
                return 0;
            }

            lock (_sync)
            {
                Type instanceType = instance.GetType();
                if (_handlesByType.TryGetValue(instanceType, out ulong existingHandle))
                {
                    _systemInstances[existingHandle] = instance;
                    return existingHandle;
                }

                ulong handle = _nextSystemHandle++;
                _systemTypes[handle] = instanceType;
                _handlesByType[instanceType] = handle;
                _systemInstances[handle] = instance;
                Logging.LogInternal($"[SystemDiscovery] Registered system instance: {instanceType.Name} (handle={handle})", LogLevel.Info);
                return handle;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[SystemDiscovery] Error registering instance: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }
}


