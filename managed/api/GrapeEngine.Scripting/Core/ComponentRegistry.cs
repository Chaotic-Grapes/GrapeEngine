/* Start Header *****************************************************************/
/*!
\file   ComponentRegistry.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Component registration API for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Internal.Hosting;
using GrapeEngine.Scripting.Internal;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Component registration utilities for C# systems.
/// Use this to register C# component types with the native ECS.
/// </summary>
public static partial class ComponentRegistry
{
    private static readonly HashSet<uint> _registeredHashes = [];
    private static readonly Lock _lock = new();

    /// <summary>
    /// Register a component type with the native ECS.
    /// This must be called before using the component type in queries or entity operations.
    /// </summary>
    /// <typeparam name="T">Component type (must be unmanaged)</typeparam>
    /// <returns>True if registration succeeded, false if already registered</returns>
    public static bool Register<T>() where T : unmanaged
    {
        uint hash = ComponentTypeHelper.GetTypeHash<T>();
        int size = Marshal.SizeOf<T>();
        int alignment = GetAlignment<T>();

        // Extract custom name from [Component] attribute if present
        string? customName = GetComponentName(typeof(T));

        lock (_lock)
        {
            // Check local cache first
            if (_registeredHashes.Contains(hash))
                return false;

            // Register with native code, passing the custom name if available
            bool success = ComponentRegistryAPI.RegisterComponent(hash, size, alignment, customName);
            
            if (success)
            {
                _registeredHashes.Add(hash);
                string displayName = customName ?? typeof(T).Name;
                Logging.LogInternal($"[ComponentRegistry] Registered {displayName} (hash: 0x{hash:X8}, size: {size}, align: {alignment})", LogLevel.Info);
                
                // Let the managed serializer know about this managed type so it can
                // marshal bytes into a typed object for JSON serialization.
                try
                {
                    ComponentSerializer.RegisterManagedType(hash, typeof(T));
                }
                catch { /* ignore if hosting not available */ }
            }

            return success;
        }
    }

    /// <summary>
    /// Check if a component type is registered.
    /// </summary>
    /// <typeparam name="T">Component type to check</typeparam>
    /// <returns>True if registered, false otherwise</returns>
    public static bool IsRegistered<T>() where T : unmanaged
    {
        uint hash = ComponentTypeHelper.GetTypeHash<T>();
        
        lock (_lock)
        {
            if (_registeredHashes.Contains(hash))
                return true;
        }

        // Check with native code in case it was registered in C++
        return ComponentRegistryAPI.IsComponentRegistered(hash);
    }

    /// <summary>
    /// Auto-register a component type if not already registered.
    /// This is called internally before component operations.
    /// </summary>
    /// <exception cref="InvalidOperationException">If component registration fails or component is not unmanaged</exception>
    internal static void EnsureRegistered<T>() where T : unmanaged
    {
        // Explicit runtime guard: verify component is truly unmanaged
        // This catches cases where records, strings, arrays, or object references sneak into components
        if (!TypeHelper.IsUnmanagedType(typeof(T)))
        {
            throw new InvalidOperationException(
                $"Component '{typeof(T).Name}' is not unmanaged. " +
                "Components may not contain strings, arrays, or object references. " +
                "Use StringId via Strings.Intern() instead of string.");
        }

        if (!IsRegistered<T>())
        {
            uint hash = ComponentTypeHelper.GetTypeHash<T>();
            string typeName = typeof(T).Name;
            
            bool registered = Register<T>();
            if (!registered)
            {
                throw new InvalidOperationException(
                    $"Failed to register component type {typeName} (hash: 0x{hash:X8}). " +
                    $"The native ECS may not have accepted this component type. " +
                    $"Ensure the component is registered on the C++ side with the same type name and hash. " +
                    $"Check that ComponentRegistry.Register<{typeName}>() succeeded and the component size ({Marshal.SizeOf<T>()} bytes) is compatible.");
            }
            
            // Log successful registration with hash for debugging type mismatches
            Logging.LogInternal($"[ComponentRegistry] Auto-registered {typeName} (hash: 0x{hash:X8})", LogLevel.Debug);
        }
    }

    /// <summary>
    /// Clear the component registry cache.
    /// Called during hot reload to allow re-registration of modified component types.
    /// </summary>
    internal static void ClearRegistrationCache()
    {
        lock (_lock)
        {
            _registeredHashes.Clear();
            Logging.LogInternal("[ComponentRegistry] Cleared registration cache for hot reload", LogLevel.Info);
        }
    }

    /// <summary>
    /// Get alignment requirement for a type.
    /// </summary>
    private static unsafe int GetAlignment<T>() where T : unmanaged
    {
        // C# doesn't expose alignment directly, so we infer from size
        // For simple types, alignment is usually the same as size (up to pointer size)
        int size = sizeof(T);
        
        // Common alignments: 1, 2, 4, 8, 16
        if (size >= 16) return 16;
        if (size >= 8) return 8;
        if (size >= 4) return 4;
        if (size >= 2) return 2;
        return 1;
    }

    /// <summary>
    /// Get the custom component name from the [Component] attribute.
    /// </summary>
    private static string? GetComponentName(Type componentType)
    {
        // Look for [Component] attribute without referencing its type directly
        var componentAttr = componentType.GetCustomAttributes()
            .FirstOrDefault(attr => attr.GetType().Name == "ComponentAttribute");

        // Get default name if no attribute found (from type name)
        // Remove "Component" suffix
        var displayName = componentType.Name.Replace("Component", "");

        // Insert spaces before capital letters for readability
        displayName = ComponentNameRegex().Replace(displayName, " $1");

        // However, if the attribute specifies a custom name, use that instead
        if (componentAttr is ComponentAttribute attribute && !string.IsNullOrWhiteSpace(attribute.Name))
            displayName = attribute.Name;

        return displayName;
    }

    [GeneratedRegex("(?<=[a-z])([A-Z])")]
    private static partial Regex ComponentNameRegex();

}

