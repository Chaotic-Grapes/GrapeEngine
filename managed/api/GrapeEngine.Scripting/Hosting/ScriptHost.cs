/* Start Header *****************************************************************/
/*!
\file   ScriptHost.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Script host for managing C# assemblies and systems. Provides functions for loading,
unloading, and reloading assemblies, discovering scripted systems, and invoking their
lifecycle methods.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Reflection;
using System.Runtime.Loader;
using System.IO;

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
    // Saved serialized state for hot-reload: assemblyPath -> (typeFullName -> blob)
    private static readonly Dictionary<string, Dictionary<string, byte[]?>> s_savedSystemStateByAssemblyPath = new();

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

            // Before removing, capture state for IHotReloadable instances
            foreach (var handle in systemHandlesToRemove)
            {
                if (s_systemInstances.TryGetValue(handle, out var inst))
                {
                    try
                    {
                        if (inst is IHotReloadable hot)
                        {
                            byte[]? blob = null;
                            try { blob = hot.OnBeforeUnload(); } catch (Exception ex) { Console.WriteLine($"[ScriptHost] Error OnBeforeUnload: {ex.Message}"); }

                            if (blob != null)
                            {
                                string assemblyPathKey = entry.Assembly.Location ?? assemblyPath;
                                if (!s_savedSystemStateByAssemblyPath.TryGetValue(assemblyPathKey, out var map))
                                {
                                    map = new Dictionary<string, byte[]?>();
                                    s_savedSystemStateByAssemblyPath[assemblyPathKey] = map;
                                }
                                map[inst.GetType().FullName ?? inst.GetType().Name] = blob;
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"[ScriptHost] Error capturing state: {ex.Message}");
                    }
                }

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
        return ReloadAssemblyImpl(assemblyPathPtr);
    }

    private static unsafe int ReloadAssemblyImpl(char* assemblyPathPtr)
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
    /// Compile all .cs files in a directory into an assembly using Roslyn.
    /// Called from C++ to compile scripts in-editor.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int CompileScriptsInDirectory(char* scriptsDirPtr, char* outputAssemblyPathPtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string outPath = Marshal.PtrToStringUTF8((IntPtr)outputAssemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] CompileScriptsInDirectory: {dir} -> {outPath}");

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            return res;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] CompileScriptsInDirectory error: {ex}");
            return -1;
        }
    }

    /// <summary>
    /// Compile directory and return pointer to UTF8 diagnostics string (allocated with CoTaskMemAlloc).
    /// Caller must free the returned pointer using FreeStringFromManaged.
    /// Returns IntPtr.Zero on failure to allocate.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe IntPtr CompileDirectoryWithDiagnostics(char* scriptsDirPtr, char* outputAssemblyPathPtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string outPath = Marshal.PtrToStringUTF8((IntPtr)outputAssemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] CompileDirectoryWithDiagnostics: {dir} -> {outPath}");

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);

            string diags = RoslynCompiler.GetLastDiagnostics() ?? string.Empty;
            // Encode as UTF8 and allocate unmanaged memory
            var bytes = System.Text.Encoding.UTF8.GetBytes(diags + '\0');
            IntPtr p = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, p, bytes.Length);

            return p;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] CompileDirectoryWithDiagnostics error: {ex}");
            return IntPtr.Zero;
        }
    }

    [UnmanagedCallersOnly]
    public static unsafe void FreeStringFromManaged(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero) return;
        Marshal.FreeHGlobal(ptr);
    }

    /// <summary>
    /// Compile scripts and reload resulting assembly.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int CompileAndReload(char* scriptsDirPtr, char* outputAssemblyPathPtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string outPath = Marshal.PtrToStringUTF8((IntPtr)outputAssemblyPathPtr) ?? "";

            Console.WriteLine($"[ScriptHost] CompileAndReload: {dir} -> {outPath}");

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Console.WriteLine("[ScriptHost] Compilation failed, aborting reload.");
                return -1;
            }

            // Call ReloadAssembly on the resulting assembly path
            // Convert managed string to UTF8 pointer (portable fallback)
            IntPtr utf8Ptr = StringToHGlobalUtf8(outPath);
            try
            {
                return ReloadAssemblyImpl((char*)utf8Ptr);
            }
            finally
            {
                if (utf8Ptr != IntPtr.Zero) Marshal.FreeHGlobal(utf8Ptr);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] CompileAndReload error: {ex}");
            return -1;
        }
    }

    /// <summary>
    /// Generate a minimal .csproj file in the specified directory to help external IDEs
    /// (VS / Rider) open the script folder. Writes to <dir>/<projectName>.csproj.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int GenerateCsProj(char* scriptsDirPtr, char* projectNamePtr)
    {
        try
        {
            string dir = Marshal.PtrToStringUTF8((IntPtr)scriptsDirPtr) ?? "";
            string projectName = Marshal.PtrToStringUTF8((IntPtr)projectNamePtr) ?? "ScriptsProject";

            if (string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir))
            {
                Console.WriteLine($"[ScriptHost] GenerateCsProj: invalid dir {dir}");
                return -1;
            }

            string outPath = Path.Combine(dir, projectName + ".csproj");

            string template = @"<Project Sdk=""Microsoft.NET.Sdk""> 
                                    <PropertyGroup>
                                        <TargetFramework>net9.0</TargetFramework>
                                        <ImplicitUsings>enable</ImplicitUsings>
                                        <Nullable>enable</Nullable>
                                    </PropertyGroup>
                                    <ItemGroup>
                                        <Compile Include=""**\*.cs"" />
                                    </ItemGroup>
                                </Project>
                             ";

            File.WriteAllText(outPath, template);
            Console.WriteLine($"[ScriptHost] Generated csproj: {outPath}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] GenerateCsProj error: {ex.Message}");
            return -1;
        }
    }

    private static IntPtr StringToHGlobalUtf8(string? s)
    {
        if (s == null) return IntPtr.Zero;
        var bytes = System.Text.Encoding.UTF8.GetBytes(s + '\0');
        IntPtr p = Marshal.AllocHGlobal(bytes.Length);
        Marshal.Copy(bytes, 0, p, bytes.Length);
        return p;
    }

    /// <summary>
    /// Managed wrapper that compiles scripts in `dir` and reloads the resulting assembly at `outPath`.
    /// This is callable from other managed classes (e.g., file watcher).
    /// </summary>
    public static unsafe int TriggerCompileAndReloadManaged(string dir, string outPath)
    {
        try
        {
            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Console.WriteLine("[ScriptHost] Compilation failed in TriggerCompileAndReloadManaged");
                return res;
            }

            IntPtr utf8Ptr = StringToHGlobalUtf8(outPath);
            try
            {
                return ReloadAssemblyImpl((char*)utf8Ptr);
            }
            finally
            {
                if (utf8Ptr != IntPtr.Zero) Marshal.FreeHGlobal(utf8Ptr);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptHost] TriggerCompileAndReloadManaged error: {ex}");
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
            
            // Create instances for each system type (and return instance handles)
            var instanceHandles = new ulong[systemTypes.Count];
            for (int i = 0; i < systemTypes.Count; i++)
            {
                Type systemType = systemTypes[i];
                
                // Create instance
                object? instance = null;
                try
                {
                    instance = Activator.CreateInstance(systemType);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptHost] Failed to create instance of {systemType.FullName}: {ex.Message}");
                    continue;
                }
                
                if (instance == null)
                {
                    Console.WriteLine($"[ScriptHost] Failed to create instance of {systemType.FullName}");
                    continue;
                }
                
                // Create instance handle
                ulong instanceHandle = s_nextSystemHandle++;
                s_systemTypes[instanceHandle] = systemType;
                s_systemInstances[instanceHandle] = instance;
                instanceHandles[i] = instanceHandle;
                
                Console.WriteLine($"[ScriptHost]   - {systemType.FullName} (handle: {instanceHandle})");
            }
            
            // Allocate unmanaged array for handles
            IntPtr handlesPtr = Marshal.AllocHGlobal(sizeof(ulong) * instanceHandles.Length);
            Marshal.Copy(instanceHandles.Select(h => (long)h).ToArray(), 0, handlesPtr, instanceHandles.Length);
            
            *outCount = instanceHandles.Length;
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

            // If we have saved state from a previous unload of the same assembly, restore it
            try
            {
                // Find the assembly path that contains this type
                string? assemblyPath = systemType.Assembly.Location;
                if (!string.IsNullOrEmpty(assemblyPath) && s_savedSystemStateByAssemblyPath.TryGetValue(assemblyPath, out var map))
                {
                    string key = systemType.FullName ?? systemType.Name;
                    if (map.TryGetValue(key, out var blob))
                    {
                        if (instance is IHotReloadable hot)
                        {
                            try { hot.OnAfterReload(blob); } catch (Exception ex) { Console.WriteLine($"[ScriptHost] Error OnAfterReload: {ex.Message}"); }
                        }
                        // Remove saved blob once applied
                        map.Remove(key);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptHost] Error restoring state for {typeName}: {ex.Message}");
            }
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
