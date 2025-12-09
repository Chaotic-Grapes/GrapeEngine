/* Start Header *****************************************************************/
/*!
\file   SystemDiscovery.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Discovers and manages ISystem implementations via reflection.
*/
/* End Header *******************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
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
    private static readonly Dictionary<ulong, Type> s_systemTypes = [];

    /// <summary>
    /// Instantiated system objects.
    /// Key: Handle
    /// Value: System instance
    /// </summary>
    private static readonly Dictionary<ulong, object> s_systemInstances = [];

    /// <summary>
    /// Next available system handle.
    /// </summary>
    private static ulong s_nextSystemHandle = 1;

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
                ulong handle = s_nextSystemHandle++;
                s_systemTypes[handle] = systemType;

                result.Add($"{handle}:{systemType.FullName}");
                Console.WriteLine($"[SystemDiscovery] Discovered system: {systemType.FullName} (handle={handle})");
            }

            return result.ToArray();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[SystemDiscovery] Error discovering systems: {ex.Message}");
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
            if (!s_systemTypes.TryGetValue(handle, out var systemType))
            {
                Console.WriteLine($"[SystemDiscovery] System not found: handle={handle}");
                return null;
            }

            // Create instance using parameterless constructor
            var instance = Activator.CreateInstance(systemType);
            s_systemInstances[handle] = instance!;

            Console.WriteLine($"[SystemDiscovery] Created system instance: {systemType.Name} (handle={handle})");
            return instance;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[SystemDiscovery] Error creating system instance: {ex.Message}");
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
                Console.WriteLine($"[SystemDiscovery] Type does not implement ISystem: {systemType.Name}");
                return 0;
            }

            // Allocate a handle
            ulong handle = s_nextSystemHandle++;
            s_systemTypes[handle] = systemType;

            // Create instance
            var instance = Activator.CreateInstance(systemType);
            if (instance == null)
            {
                s_systemTypes.Remove(handle);
                Console.WriteLine($"[SystemDiscovery] Failed to instantiate: {systemType.Name}");
                return 0;
            }

            s_systemInstances[handle] = instance;
            Console.WriteLine($"[SystemDiscovery] Created system instance: {systemType.Name} (handle={handle})");
            return handle;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[SystemDiscovery] Error creating system from type: {ex.Message}");
            return 0;
        }
    }

    /// <summary>
    /// Get a previously instantiated system by handle.
    /// </summary>
    public static object? GetSystemInstance(ulong handle)
    {
        return s_systemInstances.TryGetValue(handle, out var instance) ? instance : null;
    }

    /// <summary>
    /// Get the type of a discovered system by handle.
    /// </summary>
    public static Type? GetSystemType(ulong handle)
    {
        return s_systemTypes.TryGetValue(handle, out var type) ? type : null;
    }

    /// <summary>
    /// Clear all discovered systems and instances.
    /// Used during assembly reload.
    /// </summary>
    public static void ClearDiscoveredSystems()
    {
        s_systemTypes.Clear();
        s_systemInstances.Clear();
        s_nextSystemHandle = 1;
    }

    /// <summary>
    /// Get all currently instantiated system instances.
    /// </summary>
    public static IEnumerable<(ulong Handle, object Instance)> GetAllSystemInstances()
    {
        return s_systemInstances.Select(x => (x.Key, x.Value));
    }
}
