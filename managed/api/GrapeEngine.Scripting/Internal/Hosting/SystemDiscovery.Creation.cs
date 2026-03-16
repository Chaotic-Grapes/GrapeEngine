using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using GrapeEngine.Scripting.Systems;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static partial class SystemDiscovery
{
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
        catch (Exception ex) when (IsRecoverableSystemException(ex))
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
        catch (Exception ex) when (IsRecoverableSystemException(ex))
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
        catch (Exception ex) when (IsRecoverableSystemException(ex))
        {
            Logging.LogInternal($"[SystemDiscovery] Error creating system from type: {ex.Message}", LogLevel.Error);
            return 0;
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
            {
                return 0;
            }

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
        catch (Exception ex) when (IsRecoverableSystemException(ex))
        {
            Logging.LogInternal($"[SystemDiscovery] Error registering instance: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }
}
