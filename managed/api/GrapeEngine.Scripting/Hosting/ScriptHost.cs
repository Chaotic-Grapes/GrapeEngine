// ScriptHost.cs - Managed entry point for C++ ScriptManager
// This assembly is loaded by ScriptManager and provides the bridge between
// native C++ and managed C# scripting systems.

using System.Runtime.InteropServices;
using System.Reflection;
using System.Runtime.Loader;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Custom AssemblyLoadContext for hot reload support.
/// Allows assemblies to be unloaded and reloaded.
/// </summary>
internal class ScriptLoadContext(string assemblyPath) : AssemblyLoadContext(isCollectible: true)
{
    private readonly AssemblyDependencyResolver _resolver = new(assemblyPath);

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        string? assemblyPath = _resolver.ResolveAssemblyToPath(assemblyName);
        if (assemblyPath != null)
        {
            return LoadFromAssemblyPath(assemblyPath);
        }
        return null;
    }

    protected override IntPtr LoadUnmanagedDll(string unmanagedDllName)
    {
        string? libraryPath = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
        if (libraryPath != null)
        {
            return LoadUnmanagedDllFromPath(libraryPath);
        }
        return IntPtr.Zero;
    }
}

/// <summary>
/// Main entry point for script hosting from C++.
/// ScriptManager calls these functions to load assemblies, discover systems, etc.
/// </summary>
public static class ScriptHost
{
    // Loaded assemblies and their load contexts (for hot reload support)
    private static readonly Dictionary<string, (Assembly Assembly, ScriptLoadContext? Context)> s_loadedAssemblies = [];
    
    // Discovered system types
    private static readonly Dictionary<ulong, Type> s_systemTypes = [];
    private static readonly Dictionary<ulong, object> s_systemInstances = [];
    private static ulong s_nextSystemHandle = 1;

    /// <summary>
    /// Load a C# assembly containing scripted systems.
    /// Called from C++ ScriptManager::LoadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int LoadAssembly(char* assemblyPathPtr)
        => LoadAssemblyImpl(assemblyPathPtr);

    private static unsafe int LoadAssemblyImpl(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";
            
            Console.WriteLine($"[ScriptHost] Loading assembly: {assemblyPath}");
            
            // Create a new load context for hot reload support
            var loadContext = new ScriptLoadContext(assemblyPath);
            Assembly assembly = loadContext.LoadFromAssemblyPath(assemblyPath);
            
            s_loadedAssemblies[assemblyPath] = (assembly, loadContext);
            
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
    /// Unload an assembly for hot reload support.
    /// Called from C++ ScriptManager::UnloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int UnloadAssembly(char* assemblyPathPtr) 
        => UnloadAssemblyImpl(assemblyPathPtr);

    private static unsafe int UnloadAssemblyImpl(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] Unloading assembly: {assemblyPath}");

            if (!s_loadedAssemblies.TryGetValue(assemblyPath, out var entry))
            {
                Console.WriteLine($"[ScriptHost] Assembly not loaded: {assemblyPath}");
                return -1;
            }

            // Remove all system instances from this assembly
            var systemHandlesToRemove = new List<ulong>();
            foreach (var kvp in s_systemTypes)
            {
                if (kvp.Value.Assembly == entry.Assembly)
                {
                    systemHandlesToRemove.Add(kvp.Key);
                }
            }

            foreach (var handle in systemHandlesToRemove)
            {
                s_systemInstances.Remove(handle);
                s_systemTypes.Remove(handle);
                Console.WriteLine($"[ScriptHost] Removed system instance: handle={handle}");
            }

            // Unload the assembly load context
            if (entry.Context != null)
            {
                entry.Context.Unload();
                Console.WriteLine($"[ScriptHost] Unloaded context for: {assemblyPath}");
            }

            s_loadedAssemblies.Remove(assemblyPath);

            // Force garbage collection to reclaim memory
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();

            Console.WriteLine($"[ScriptHost] Successfully unloaded: {assemblyPath}");
            return 0; // Success
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error unloading assembly: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Reload an assembly for hot reload support.
    /// This unloads the old version and loads the new one.
    /// Called from C++ ScriptManager::ReloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int ReloadAssembly(char* assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8((IntPtr)assemblyPathPtr) ?? "";
            
            Console.WriteLine($"[ScriptHost] Reloading assembly: {assemblyPath}");

            // Unload existing assembly
            int unloadResult = UnloadAssemblyImpl(assemblyPathPtr);
            if (unloadResult != 0)
            {
                Console.WriteLine($"[ScriptHost] Warning: Failed to unload existing assembly during reload");
            }
            
            // Wait a bit for finalizers to complete
            System.Threading.Thread.Sleep(100);
            
            // Load new version
            int loadResult = LoadAssemblyImpl(assemblyPathPtr);
            if (loadResult != 0)
            {
                Console.WriteLine($"[ScriptHost] Failed to load new assembly during reload");
                return -1;
            }
            
            Console.WriteLine($"[ScriptHost] Successfully reloaded: {assemblyPath}");
            return 0; // Success
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] Error reloading assembly: {ex.Message}");
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
            foreach (var entry in s_loadedAssemblies.Values)
            {
                try
                {
                    var types = entry.Assembly.GetTypes()
                        .Where(t => t.IsClass && !t.IsAbstract)
                        .Where(t => typeof(ISystem).IsAssignableFrom(t));
                    
                    systemTypes.AddRange(types);
                }
                catch (ReflectionTypeLoadException ex)
                {
                    Console.WriteLine($"[ScriptHost] Warning: Could not load some types from {entry.Assembly.FullName}");
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
            foreach (var entry in s_loadedAssemblies.Values)
            {
                systemType = entry.Assembly.GetType(typeName);
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
            Marshal.Copy(nameBytes, 0, (IntPtr)outNameBuffer, System.Math.Min(nameBytes.Length, 256));
            
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
                // Wrap native World pointer in managed World wrapper
                World managedWorld = new World(worldPtr);
                Console.WriteLine($"[ScriptHost] CallSystemOnCreate for {instance.GetType().Name}");
                system.OnCreate(managedWorld);
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
                // Wrap native World pointer in managed World wrapper
                World managedWorld = new World(worldPtr);
                system.OnUpdate(managedWorld, deltaTime);
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
                // Wrap native World pointer in managed World wrapper
                World managedWorld = new World(worldPtr);
                system.OnDestroy(managedWorld);
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
