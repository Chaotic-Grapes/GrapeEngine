// ScriptHost.cs - Managed entry point for C++ ScriptManager
// This assembly is loaded by ScriptManager and provides the bridge between
// native C++ and managed C# scripting systems.

using System.Runtime.InteropServices;
using System.Reflection;

namespace GrapeEngine.ScriptHost;

/// <summary>
/// Main entry point for script hosting from C++.
/// ScriptManager calls these functions to load assemblies, discover systems, etc.
/// </summary>
public static class ScriptHost
{
    // Loaded assemblies (for hot reload support)
    private static readonly Dictionary<string, Assembly> s_loadedAssemblies = new();
    
    // Discovered system types
    private static readonly Dictionary<ulong, Type> s_systemTypes = new();
    private static readonly Dictionary<ulong, object> s_systemInstances = new();
    private static ulong s_nextSystemHandle = 1;

    /// <summary>
    /// Load a C# assembly containing scripted systems.
    /// Called from C++ ScriptManager::LoadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int LoadAssembly(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";
            
            Console.WriteLine($"[ScriptHost] Loading assembly: {assemblyPath}");
            
            // Load assembly
            Assembly assembly = Assembly.LoadFrom(assemblyPath);
            s_loadedAssemblies[assemblyPath] = assembly;
            
            Console.WriteLine($"[ScriptHost] Loaded: {assembly.FullName}");
            return 0; // Success
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Failed to load assembly: {ex.Message}");
            return -1; // Failure
        }
    }

    /// <summary>
    /// Unload an assembly (requires AssemblyLoadContext for true unloading).
    /// Called from C++ ScriptManager::UnloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int UnloadAssembly(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";
            
            // TODO: Implement with AssemblyLoadContext for hot reload
            Console.WriteLine($"[ScriptHost] UnloadAssembly not yet implemented: {assemblyPath}");
            
            return -1; // Not implemented
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error unloading assembly: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Discover all ISystem implementations in loaded assemblies.
    /// Called from C++ ScriptManager::DiscoverScriptedSystems()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void* DiscoverSystems(int* outCount)
    {
        try
        {
            Console.WriteLine("[ScriptHost] Discovering systems in loaded assemblies...");
            
            var systemTypes = new List<Type>();
            
            // Search all loaded assemblies for ISystem implementations
            foreach (var assembly in s_loadedAssemblies.Values)
            {
                try
                {
                    var types = assembly.GetTypes()
                        .Where(t => t.IsClass && !t.IsAbstract)
                        .Where(t => typeof(ISystem).IsAssignableFrom(t));
                    
                    systemTypes.AddRange(types);
                }
                catch (ReflectionTypeLoadException ex)
                {
                    Console.WriteLine($"[ScriptHost] Warning: Could not load some types from {assembly.FullName}");
                    Console.WriteLine($"  Errors: {string.Join(", ", ex.LoaderExceptions.Select(e => e?.Message))}");
                }
            }
            
            Console.WriteLine($"[ScriptHost] Found {systemTypes.Count} system types");
            
            // Create handles for each system type
            var handles = new ulong[systemTypes.Count];
            for (int i = 0; i < systemTypes.Count; i++)
            {
                ulong handle = s_nextSystemHandle++;
                s_systemTypes[handle] = systemTypes[i];
                handles[i] = handle;
                
                Console.WriteLine($"[ScriptHost]   - {systemTypes[i].FullName} (handle: {handle})");
            }
            
            // Allocate unmanaged array for handles
            IntPtr handlesPtr = Marshal.AllocHGlobal(sizeof(ulong) * handles.Length);
            Marshal.Copy(handles.Select(h => (long)h).ToArray(), 0, handlesPtr, handles.Length);
            
            *outCount = handles.Length;
            return (void*)handlesPtr;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error discovering systems: {ex.Message}");
            *outCount = 0;
            return null;
        }
    }

    /// <summary>
    /// Create an instance of a scripted system.
    /// Called from C++ when creating ScriptSystemWrapper.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe ulong CreateSystemInstance(char* typeNamePtr)
    {
        try
        {
            string typeName = Marshal.PtrToStringUTF8((IntPtr)typeNamePtr) ?? "";
            
            // Find the type
            Type? systemType = null;
            foreach (var assembly in s_loadedAssemblies.Values)
            {
                systemType = assembly.GetType(typeName);
                if (systemType != null) break;
            }
            
            if (systemType == null)
            {
                Console.WriteLine($"[ScriptHost] System type not found: {typeName}");
                return 0;
            }
            
            // Create instance
            object? instance = Activator.CreateInstance(systemType);
            if (instance == null)
            {
                Console.WriteLine($"[ScriptHost] Failed to create instance of: {typeName}");
                return 0;
            }
            
            // Store and return handle
            ulong handle = s_nextSystemHandle++;
            s_systemInstances[handle] = instance;
            s_systemTypes[handle] = systemType;
            
            Console.WriteLine($"[ScriptHost] Created system instance: {typeName} (handle: {handle})");
            return handle;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error creating system instance: {ex.Message}");
            return 0;
        }
    }

    /// <summary>
    /// Destroy a system instance.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void DestroySystemInstance(ulong handle)
    {
        s_systemInstances.Remove(handle);
        s_systemTypes.Remove(handle);
    }

    /// <summary>
    /// Get metadata about a system (name, group, run mode).
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void GetSystemMetadata(ulong handle, char* outNameBuffer, int* outGroup, int* outRunMode)
    {
        try
        {
            if (!s_systemTypes.TryGetValue(handle, out Type? systemType))
            {
                Console.WriteLine($"[ScriptHost] System handle not found: {handle}");
                return;
            }
            
            // Get name
            string name = systemType.FullName ?? systemType.Name;
            byte[] nameBytes = System.Text.Encoding.UTF8.GetBytes(name);
            Marshal.Copy(nameBytes, 0, (IntPtr)outNameBuffer, Math.Min(nameBytes.Length, 256));
            
            // TODO: Get group and run mode from attributes or interface methods
            *outGroup = (int)SystemGroup.Update;
            *outRunMode = (int)SystemRunMode.PlayOnly;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error getting metadata: {ex.Message}");
        }
    }

    /// <summary>
    /// Call OnCreate on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void CallSystemOnCreate(ulong handle, void* worldPtr)
    {
        try
        {
            if (!s_systemInstances.TryGetValue(handle, out object? instance))
            {
                Console.WriteLine($"[ScriptHost] System instance not found: {handle}");
                return;
            }
            
            if (instance is ISystem system)
            {
                // TODO: Wrap worldPtr in managed World wrapper
                Console.WriteLine($"[ScriptHost] TODO: CallSystemOnCreate for {instance.GetType().Name}");
                // system.OnCreate(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in OnCreate: {ex.Message}");
        }
    }

    /// <summary>
    /// Call OnUpdate on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void CallSystemOnUpdate(ulong handle, void* worldPtr, float deltaTime)
    {
        try
        {
            if (!s_systemInstances.TryGetValue(handle, out object? instance))
            {
                return;
            }
            
            if (instance is ISystem system)
            {
                // TODO: Wrap worldPtr in managed World wrapper
                // system.OnUpdate(managedWorld, deltaTime);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in OnUpdate: {ex.Message}");
        }
    }

    /// <summary>
    /// Call OnDestroy on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void CallSystemOnDestroy(ulong handle, void* worldPtr)
    {
        try
        {
            if (!s_systemInstances.TryGetValue(handle, out object? instance))
            {
                return;
            }
            
            if (instance is ISystem system)
            {
                // TODO: Wrap worldPtr in managed World wrapper
                // system.OnDestroy(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error in OnDestroy: {ex.Message}");
        }
    }
}

// Placeholder enums (should match C++ definitions)
public enum SystemGroup
{
    PreUpdate,
    Update,
    PostUpdate,
    PrePhysics,
    Physics,
    PostPhysics,
    PreRender,
    Render,
    PostRender
}

public enum SystemRunMode
{
    Always,
    PlayOnly,
    EditOnly
}

// Placeholder ISystem interface (will be moved to ScriptAPI)
public interface ISystem
{
    void OnCreate(object world);
    void OnUpdate(object world, float deltaTime);
    void OnDestroy(object world);
}
