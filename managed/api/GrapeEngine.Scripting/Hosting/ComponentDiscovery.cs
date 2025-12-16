/* Start Header *****************************************************************/
/*!
\file   ComponentDiscovery.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Discovers and registers all C# component types with the native ECS.

This system performs eager discovery of all unmanaged struct components in loaded
assemblies at startup. Each discovered component is registered with the native ECS
via reflection so it can be used in queries, entity operations, and the inspector
immediately without runtime laziness.

The discovery process:
1. Enumerate all loaded assemblies in the script context
2. Find all unmanaged struct types (potential components)
3. Register each with the native C++ ComponentRegistry via ComponentRegistry.Register<T>()
4. The editor automatically discovers registered components by querying the native ECS
*/
/* End Header *******************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Components.Core;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Discovers C# component types and registers them with the native ECS and editor.
/// 
/// This runs at script host initialization to ensure all components are available
/// for editor UI operations and queries without waiting for runtime usage.
/// </summary>
internal static class ComponentDiscovery
{
    /// <summary>
    /// Discover all unmanaged component types in loaded assemblies and register them.
    /// Called from ScriptHost initialization.
    /// </summary>
    public static void DiscoverAndRegisterAll()
    {
        var components = DiscoverComponents();
        Console.WriteLine($"[ComponentDiscovery] Found {components.Count} component types");

        foreach (var componentType in components)
        {
            RegisterComponent(componentType);
        }

        Console.WriteLine("[ComponentDiscovery] Component discovery complete");
    }

    /// <summary>
    /// Find all unmanaged struct types that are components.
    /// </summary>
    private static List<Type> DiscoverComponents()
    {
        var components = new List<Type>();

        // Get all loaded assemblies in the current script context
        var assemblies = AppDomain.CurrentDomain.GetAssemblies()
            .Where(a => !a.IsDynamic && a.GetName().Name?.StartsWith("GrapeEngine") == true)
            .ToList();

        foreach (var assembly in assemblies)
        {
            try
            {
                var types = assembly.GetTypes()
                    .Where(IsComponent)
                    .ToList();

                components.AddRange(types);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ComponentDiscovery] Failed to scan assembly {assembly.GetName().Name}: {ex.Message}");
            }
        }

        return components;
    }

    /// <summary>
    /// Check if a type is a valid component (unmanaged struct).
    /// </summary>
    private static bool IsComponent(Type type)
    {
        // Must be a value type (struct)
        if (!type.IsValueType)
            return false;

        // Must not be a primitive or built-in type
        if (type.IsPrimitive || type.IsEnum)
            return false;

        // Skip special types
        if (type.Namespace?.StartsWith("System") == true)
            return false;

        // Must be unmanaged (blittable)
        try
        {
            var method = typeof(Type).GetMethod("IsUnmanagedType", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            if (method != null)
            {
                var isUnmanaged = (bool)method.Invoke(type, null)!;
                if (!isUnmanaged)
                    return false;
            }
            else
            {
                // Fallback: check for StructLayout attribute
                if (type.GetCustomAttribute<StructLayoutAttribute>() == null)
                    return false;
            }
        }
        catch
        {
            return false;
        }

        return true;
    }

    /// <summary>
    /// Register a single component type with C# runtime and native systems.
    /// </summary>
    private static void RegisterComponent(Type componentType)
    {
        try
        {
            // Register with C# ComponentRegistry
            var registerMethod = typeof(ComponentRegistry)
                .GetMethod("Register", BindingFlags.Public | BindingFlags.Static)
                ?.MakeGenericMethod(componentType);

            registerMethod?.Invoke(null, Array.Empty<object>());

            // Get component metadata for logging
            var getHashMethod = typeof(ComponentTypeHelper)
                .GetMethod("GetTypeHash", BindingFlags.Public | BindingFlags.Static)
                ?.MakeGenericMethod(componentType);

            if (getHashMethod != null)
            {
                var hash = (uint)getHashMethod.Invoke(null, Array.Empty<object>())!;
                Console.WriteLine($"[ComponentDiscovery] Registered {componentType.Name} (hash: 0x{hash:X8})");
            }
            else
            {
                Console.WriteLine($"[ComponentDiscovery] Registered {componentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ComponentDiscovery] Failed to register {componentType.Name}: {ex.Message}");
        }
    }

}
