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

using System.Reflection;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// Discovers C# component types and registers them with the native ECS and editor.
/// 
/// This runs at script host initialization to ensure all components are available
/// for editor UI operations and queries without waiting for runtime usage.
/// </summary>
internal static class ComponentDiscovery
{
    /// <summary>
    /// Maps component type hashes to their Type objects.
    /// Used by the editor when deserializing component values from JSON at runtime.
    /// </summary>
    public static Dictionary<uint, Type> TypeHashToType { get; } = new();
    /// <summary>
    /// Discover all unmanaged component types in loaded assemblies and register them.
    /// Called from ScriptHost initialization.
    /// </summary>
    public static void DiscoverAndRegisterAll()
    {
        // NOTE: Do NOT clear the registration cache here!
        // It must be cleared AFTER ClearAllManagedComponentsFromEntities() completes,
        // so that component removal can still find the old component hashes/IDs.
        // The cache will be cleared by the caller after component cleanup.

        var components = DiscoverComponents();
        Logging.LogInternal($"[ComponentDiscovery] Found {components.Count} component types", LogLevel.Info);

        foreach (var componentType in components)
        {
            RegisterComponent(componentType);
        }
        
        // Also force register all GrapeEngine.Scripting components for backwards compatibility
        // These might not have [Component] attribute but still need to be registered
        RegisterGrapeEngineComponents();
    }

    /// <summary>
    /// Find all unmanaged struct types that are components.
    /// </summary>
    private static List<Type> DiscoverComponents()
    {
        var components = new List<Type>();

        // Get all loaded assemblies in the current script context
        // Include all kinds of assemblies and user script assemblies (e.g., GameScripts)
        var allAssemblies = AppDomain.CurrentDomain.GetAssemblies();
        Logging.LogInternal($"[ComponentDiscovery] Total assemblies loaded: {allAssemblies.Length}", LogLevel.Info);
        
        var assemblies = allAssemblies
            .Where(a => !a.IsDynamic && ShouldScanAssembly(a))
            .ToList();
        
        Logging.LogInternal($"[ComponentDiscovery] Assemblies to scan: {assemblies.Count}", LogLevel.Info);

        foreach (var assembly in assemblies)
        {
            // Skip System assemblies and GrapeEngine assemblies
            if (assembly.GetName()?.Name is { } asmName &&
                (asmName.StartsWith("System") ||
                 asmName.StartsWith("Microsoft") ||
                 asmName.StartsWith("GrapeEngine")))
                continue;

            try
            {
                // Commented is for logging purposes only
                // NOTE: If uncommented, the editor may take longer time to start due to the extra logging

                // === Logging for debugging ===
                // var allTypes = assembly.GetTypes();
                // Logging.LogInternal($"[ComponentDiscovery] {assembly.GetName().Name} has {allTypes.Length} total types:", LogLevel.Info);
                // foreach (var t in allTypes)
                // {
                //     Logging.LogInternal($"[ComponentDiscovery] Type: {t.FullName}", LogLevel.Info);
                // }

                // var types = allTypes
                //     .Where(IsComponent)
                //     .ToList();

                // Logging.LogInternal($"[ComponentDiscovery] {assembly.GetName().Name}: {types.Count} components", LogLevel.Info);
                // foreach (var t in types)
                // {
                //     Logging.LogInternal($"[ComponentDiscovery] Component found: {t.FullName}", LogLevel.Info);
                // }

                // components.AddRange(types);
                // =============================

                // Directly add discovered components if not logging
                // Comment this out if you want detailed logging above
                // Filter out System types and GrapeEngine assembly types
                components.AddRange(assembly
                    .GetTypes()
                    .Where(IsComponent)
                );
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[ComponentDiscovery] Failed to scan assembly {assembly.GetName().Name}: {ex.Message}", LogLevel.Error);
            }
        }

        return components;
    }

    /// <summary>
    /// Check if a type is a valid component (has [Component] attribute and is unmanaged struct).
    /// </summary>
    private static bool IsComponent(Type type)
    {
        // Must have [Component] attribute!
        // Check by name to avoid assembly binding issues
        var hasComponentAttr = type.GetCustomAttributes()
            .Any(attr => attr.GetType().Name == "ComponentAttribute");

        Logging.LogInternal($"[ComponentDiscovery] Checking type: {type.FullName}", LogLevel.Debug);

        foreach (var attr in type.GetCustomAttributes())
        {
            //if (string.IsNullOrWhiteSpace(type.FullName) || !type.FullName.StartsWith("EchoesBelow"))
            //    continue;

            Logging.LogInternal($"[ComponentDiscovery] {type.FullName} has attribute: {attr.GetType().FullName}", LogLevel.Debug);
        }

        if (!hasComponentAttr)
        {
            return false;  // Not marked as component, skip it
        }

        // Must be a value type (struct)
        if (!type.IsValueType)
        {
            Logging.LogInternal($"[ComponentDiscovery] {type.FullName} rejected: not a value type (IsValueType={type.IsValueType})", LogLevel.Warning);
            return false;
        }

        // Must not be a primitive or built-in type
        if (type.IsPrimitive || type.IsEnum)
        {
            Logging.LogInternal($"[ComponentDiscovery] {type.FullName} rejected: is primitive or enum", LogLevel.Warning);
            return false;
        }

        // Skip special types
        if (type.Namespace?.StartsWith("System") == true)
        {
            return false;
        }

        // Must be unmanaged (blittable)
        if (!IsUnmanagedType(type))
        {
            Logging.LogInternal($"[ComponentDiscovery] {type.FullName} rejected: not unmanaged", LogLevel.Warning);
            return false;
        }

        Logging.LogInternal($"[ComponentDiscovery] {type.FullName} is a valid component!", LogLevel.Info);
        return true;
    }

    /// <summary>
    /// Check if a type is unmanaged (can be used in P/Invoke and unsafe code).
    /// </summary>
    private static bool IsUnmanagedType(Type type)
    {
        // Try the reflection method first
        try
        {
            var method = typeof(Type).GetMethod("IsUnmanagedType", BindingFlags.NonPublic | BindingFlags.Instance);
            if (method != null)
            {
                var result = (bool)method.Invoke(type, null)!;
                return result;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentDiscovery] IsUnmanagedType reflection failed: {ex.Message}", LogLevel.Warning);
        }

        // Fallback: try to use Marshal.SizeOf - if it works, the type is unmanaged
        try
        {
            Marshal.SizeOf(type);
            Logging.LogInternal($"[ComponentDiscovery] {type.FullName}: unmanaged via Marshal.SizeOf", LogLevel.Info);
            return true;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentDiscovery] {type.FullName}: Marshal.SizeOf failed: {ex.Message}", LogLevel.Warning);
        }

        // Last resort: check if it has StructLayout attribute
        var hasStructLayout = type.GetCustomAttribute<StructLayoutAttribute>() != null;
        Logging.LogInternal($"[ComponentDiscovery] {type.FullName}: StructLayout={hasStructLayout}", LogLevel.Info);
        return hasStructLayout;
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

            registerMethod?.Invoke(null, []);

            // Get component metadata for logging
            var getHashMethod = typeof(ComponentTypeHelper)
                .GetMethod("GetTypeHash", BindingFlags.Public | BindingFlags.Static)
                ?.MakeGenericMethod(componentType);

            if (getHashMethod != null)
            {
                var hash = (uint)getHashMethod.Invoke(null, [])!;
                
                // Add to the type hash mapping for editor deserialization
                TypeHashToType[hash] = componentType;
                
                Logging.LogInternal($"[ComponentDiscovery] Registered {componentType.Name} (hash: 0x{hash:X8})", LogLevel.Info);
            }
            else
            {
                Logging.LogInternal($"[ComponentDiscovery] Registered {componentType.Name}", LogLevel.Info);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentDiscovery] Failed to register {componentType.Name}: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Determine if an assembly should be scanned for components.
    /// Scans GrapeEngine.* assemblies and user script assemblies (e.g., GameScripts).
    /// Excludes System.* and other framework assemblies.
    /// </summary>
    private static bool ShouldScanAssembly(Assembly assembly)
    {
        var assemblyName = assembly.GetName().Name ?? string.Empty;

        // Always include GrapeEngine assemblies
        if (assemblyName.StartsWith("GrapeEngine"))
            return true;

        // Include user script assemblies (GameScripts, EchoesBelow, etc.)
        // Exclude System, Microsoft, and other framework assemblies
        if (assemblyName.StartsWith("System") || 
            assemblyName.StartsWith("Microsoft") ||
            assemblyName.StartsWith("netstandard") ||
            assemblyName.StartsWith("mscorlib"))
            return false;

        // Include everything else (user scripts)
        return true;
    }

    /// <summary>
    /// Force register all component types from GrapeEngine assemblies.
    /// This ensures backwards compatibility for components that may not have [Component] attribute.
    /// </summary>
    private static void RegisterGrapeEngineComponents()
    {
        var grapeEngineAssemblies = AppDomain.CurrentDomain.GetAssemblies()
            .Where(a => a.GetName().Name?.StartsWith("GrapeEngine") == true)
            .ToList();
            
        foreach (var assembly in grapeEngineAssemblies)
        {
            try
            {
                var componentTypes = assembly.GetTypes()
                    .Where(type => IsUnmanagedComponentType(type))
                    .ToList();
                    
                Logging.LogInternal($"[ComponentDiscovery] Force registering {componentTypes.Count} component types from {assembly.GetName().Name}", LogLevel.Info);
                
                foreach (var componentType in componentTypes)
                {
                    // Force register this type
                    RegisterComponent(componentType);
                }
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[ComponentDiscovery] Failed to force register from {assembly.GetName().Name}: {ex.Message}", LogLevel.Error);
            }
        }
    }

    /// <summary>
    /// Check if a type is a valid unmanaged component type regardless of [Component] attribute.
    /// Used for force registration of GrapeEngine components.
    /// </summary>
    private static bool IsUnmanagedComponentType(Type type)
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
            
        // Skip nested types (they're usually not components)
        if (type.IsNested)
            return false;

        // Must be unmanaged (blittable)
        if (!IsUnmanagedType(type))
            return false;
            
        // Must be in a Components namespace or assembly
        if (type.Namespace?.Contains("Components") != true)
            return false;

        return true;
    }

    /// <summary>
    /// Clear all cached component type mappings.
    /// Called during assembly unload to break references to types from the loaded assembly.
    /// </summary>
    public static void ClearTypeCache()
    {
        try
        {
            int count = TypeHashToType.Count;
            TypeHashToType.Clear();
            if (count > 0)
            {
                Logging.LogInternal($"[ComponentDiscovery] Cleared {count} component type cache entries", LogLevel.Info);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ComponentDiscovery] Error clearing type cache: {ex.Message}", LogLevel.Error);
        }
    }

}


