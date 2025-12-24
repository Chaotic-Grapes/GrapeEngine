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

namespace GrapeEngine.Scripting.Hosting;

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

    /// <summary>
    /// Instantiated system objects.
    /// Key: Handle
    /// Value: System instance
    /// </summary>
    private static readonly Dictionary<ulong, object> _systemInstances = [];

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
            var systemTypes = assembly
                .GetTypes()
                .Where(t => typeof(ISystem).IsAssignableFrom(t) && t is { IsInterface: false, IsAbstract: false })
                .ToList();

            var result = new List<string>();

            foreach (var systemType in systemTypes)
            {
                // Assign handle and register
                ulong handle = _nextSystemHandle++;
                _systemTypes[handle] = systemType;

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
            if (!_systemTypes.TryGetValue(handle, out var systemType))
            {
                Logging.LogInternal($"[SystemDiscovery] System not found: handle={handle}", LogLevel.Warning);
                return null;
            }

            // Create instance using parameterless constructor
            var instance = Activator.CreateInstance(systemType);
            _systemInstances[handle] = instance!;

            Logging.LogInternal($"[SystemDiscovery] Created system instance: {systemType.Name} (handle={handle})", LogLevel.Info);
            return instance;
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

            // Allocate a handle
            ulong handle = _nextSystemHandle++;
            _systemTypes[handle] = systemType;

            // Create instance
            var instance = Activator.CreateInstance(systemType);
            if (instance == null)
            {
                _systemTypes.Remove(handle);
                Logging.LogInternal($"[SystemDiscovery] Failed to instantiate: {systemType.Name}", LogLevel.Warning);
                return 0;
            }

            _systemInstances[handle] = instance;
            Logging.LogInternal($"[SystemDiscovery] Created system instance: {systemType.Name} (handle={handle})", LogLevel.Info);
            return handle;
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
        return _systemInstances.TryGetValue(handle, out var instance) ? instance : null;
    }

    /// <summary>
    /// Get the type of a discovered system by handle.
    /// </summary>
    public static Type? GetSystemType(ulong handle)
    {
        return _systemTypes.TryGetValue(handle, out var type) ? type : null;
    }

    /// <summary>
    /// Clear all discovered systems and instances.
    /// Used during assembly reload.
    /// </summary>
    public static void ClearDiscoveredSystems()
    {
        _systemTypes.Clear();
        _systemInstances.Clear();
        _nextSystemHandle = 1;
    }

    /// <summary>
    /// Get all currently instantiated system instances.
    /// </summary>
    public static IEnumerable<(ulong Handle, object Instance)> GetAllSystemInstances()
    {
        return _systemInstances.Select(x => (x.Key, x.Value));
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

            ulong handle = _nextSystemHandle++;
            _systemTypes[handle] = instance.GetType();
            _systemInstances[handle] = instance;
            Logging.LogInternal($"[SystemDiscovery] Registered system instance: {instance.GetType().Name} (handle={handle})", LogLevel.Info);
            return handle;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[SystemDiscovery] Error registering instance: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }
}
