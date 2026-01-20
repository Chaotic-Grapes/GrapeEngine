/* Start Header *****************************************************************/
/*!
\file   ScriptHost.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Script host for managing C# assemblies and systems. Main P/Invoke entry point.
Coordinates assembly loading, system discovery, and hot reload via helper classes:
- AssemblyManager: Assembly loading/unloading
- SystemDiscovery: System discovery and instantiation  
- StatePreserver: Hot reload state management

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Linq;
using GrapeEngine.Scripting.Job;

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
        // Prefer already-loaded assemblies in the default context to preserve
        // type identity for shared API assemblies (e.g. GrapeEngine.Scripting).
        var already = AppDomain.CurrentDomain.GetAssemblies()
            .FirstOrDefault(a => string.Equals(a.GetName().Name, assemblyName.Name, StringComparison.OrdinalIgnoreCase));
        if (already != null)
            return already;

        // Fallback to resolving within the custom load context
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
/// SCRIPT HOST - Main P/Invoke entry point for script hosting.
/// 
/// Delegates specialized tasks to focused helper classes:
/// - AssemblyManager: Assembly lifecycle (load/unload)
/// - SystemDiscovery: System discovery and instantiation
/// - StatePreserver: Hot reload state preservation
/// 
/// This class serves as the coordinator and C++ interface layer only.
/// </summary>
public static class ScriptHost
{
    /// <summary>
    /// Extract the original assembly path from a versioned path.
    /// GameScripts_hotreload_1.dll -> GameScripts.dll
    /// </summary>
    private static string ExtractOriginalPathFromVersioned(string versionedPath)
    {
        string dir = Path.GetDirectoryName(versionedPath) ?? "";
        string filename = Path.GetFileNameWithoutExtension(versionedPath);
        string ext = Path.GetExtension(versionedPath);

        // Remove _hotreload_X suffix
        // GameScripts_hotreload_1 -> GameScripts
        int hotreloadIndex = filename.LastIndexOf("_hotreload_");
        if (hotreloadIndex > 0)
        {
            string originalFilename = filename.Substring(0, hotreloadIndex);
            return Path.Combine(dir, originalFilename + ext);
        }

        return versionedPath; // Not a versioned path, return as-is
    }

    /// <summary>
    /// Delegate type for the native C++ callback function.
    /// Called when hot reload completes with the reloaded assembly path.
    /// </summary>
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void NativeHotReloadCallback(string assemblyPath);

    /// <summary>
    /// Callback function pointer passed from C++ native code.
    /// </summary>
    private static IntPtr _nativeHotReloadCallback = IntPtr.Zero;

    /// <summary>
    /// Cache of World wrapper objects to avoid allocating new World objects every frame.
    /// Maps native world pointer to managed World wrapper.
    /// </summary>
    private static readonly Dictionary<IntPtr, World> _worldCache = [];

    /// <summary>
    /// Get or create a cached World wrapper for a native world pointer.
    /// Reuses the same managed World object across frames to minimize allocations.
    /// </summary>
    private static World GetOrCreateWorldWrapper(IntPtr worldPtr)
    {
        if (_worldCache.TryGetValue(worldPtr, out var cachedWorld))
        {
            return cachedWorld;
        }
        
        var newWorld = new World(worldPtr);
        _worldCache[worldPtr] = newWorld;
        return newWorld;
    }

    /// <summary>
    /// Called from C++ to provide the native callback function pointer.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void RegisterHotReloadCallback(IntPtr callbackPtr)
    {
        _nativeHotReloadCallback = callbackPtr;
        Logging.LogInternal("[ScriptHost] Hot reload callback registered", LogLevel.Info);
    }

    /// <summary>
    /// Notify C++ that hot reload is complete by invoking the native callback.
    /// </summary>
    private static void NotifyHotReloadComplete(string assemblyPath)
    {
        if (_nativeHotReloadCallback != IntPtr.Zero)
        {
            try
            {
                var callback = Marshal.GetDelegateForFunctionPointer<NativeHotReloadCallback>(_nativeHotReloadCallback);
                callback?.Invoke(assemblyPath);
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[ScriptHost] Error invoking hot reload callback: {ex.Message}", LogLevel.Error);
            }
        }
    }
    /// Called from C++ ScriptManager::LoadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static int LoadAssembly(IntPtr assemblyPathPtr)
    {
        try
        {
            var assemblyPath = Marshal.PtrToStringUTF8(assemblyPathPtr) ?? string.Empty;
            var result = AssemblyManager.LoadAssembly(assemblyPath);
            
            if (result != null)
            {
                // Discover and register all components in loaded assemblies
                ComponentDiscovery.DiscoverAndRegisterAll();
                return 0;
            }
            return -1;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] LoadAssembly error: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Unload an assembly for hot reload support.
    /// Called from C++ ScriptManager::UnloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static int UnloadAssembly(IntPtr assemblyPathPtr)
    {
        try
        {
            var assemblyPath = Marshal.PtrToStringUTF8(assemblyPathPtr) ?? string.Empty;

            Logging.LogInternal($"[ScriptHost] Unloading assembly: {assemblyPath}", LogLevel.Info);

            // Save state from all systems in this assembly
            StatePreserver.SaveAllSystemStates(assemblyPath);

            // Unload assembly
            bool success = AssemblyManager.UnloadAssembly(assemblyPath);
            if (!success)
            {
                return -1;
            }

            // Clear discovered systems
            SystemDiscovery.ClearDiscoveredSystems();

            Logging.LogInternal($"[ScriptHost] Successfully unloaded: {assemblyPath}", LogLevel.Info);
            return 0; // Success
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error unloading assembly: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Reload an assembly for hot reload support.
    /// This unloads the old version and loads the new one.
    /// Called from C++ ScriptManager::ReloadAssembly()
    /// </summary>
    [UnmanagedCallersOnly]
    public static int ReloadAssembly(IntPtr assemblyPathPtr)
    {
        try
        {
            string assemblyPath = Marshal.PtrToStringUTF8(assemblyPathPtr) ?? string.Empty;
            return ReloadAssemblyInternal(assemblyPath);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error reloading assembly: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Internal implementation of assembly reload logic.
    /// Separated so it can be called from both managed and unmanaged code.
    /// </summary>
    private static int ReloadAssemblyInternal(string assemblyPath)
    {
        try
        {
            string logicalPath = ExtractOriginalPathFromVersioned(assemblyPath);

            Logging.LogInternal($"[ScriptHost] Reloading assembly: {assemblyPath} (logical: {logicalPath})", LogLevel.Info);

            // Save state before unload (keyed by logical path to survive version changes)
            StatePreserver.SaveAllSystemStates(logicalPath);

            // If an older version is loaded, unload it first.
            if (AssemblyManager.IsAssemblyLoaded(logicalPath))
            {
                Logging.LogInternal($"[ScriptHost] Unloading old assembly before reload: {logicalPath}", LogLevel.Info);
                AssemblyManager.UnloadAssembly(logicalPath);
            }

            // Load new version (can be a versioned path or an original path; AssemblyManager will resolve latest)
            if (AssemblyManager.LoadAssembly(assemblyPath) == null)
            {
                Logging.LogInternal($"[ScriptHost] Failed to load new assembly during reload", LogLevel.Error);
                return -1;
            }

            // Re-discover and register all components in loaded assemblies
            // This is CRITICAL for hot reload to work - components must be re-registered
            // so that the C++ side sees updated component structures
            ComponentDiscovery.DiscoverAndRegisterAll();
            Logging.LogInternal($"[ScriptHost] Re-discovered components after reload", LogLevel.Info);

            // Clear discovered systems before discovering new ones to avoid duplication
            SystemDiscovery.ClearDiscoveredSystems();

            // Discover systems in newly loaded assembly
            Assembly? newAssembly = AssemblyManager.GetLoadedAssembly(logicalPath);
            if (newAssembly != null)
            {
                SystemDiscovery.DiscoverSystemsInAssembly(newAssembly);
            }

            // Clean up saved state
            StatePreserver.ClearSavedState(logicalPath);

            // Clean up old versioned assemblies - but only if we loaded a versioned file
            // (i.e., if a new hot reload compilation created a new _hotreload_X.dll)
            // If we loaded the original GameScripts.dll on startup, don't delete anything
            if (newAssembly?.Location?.Contains("_hotreload_", StringComparison.OrdinalIgnoreCase) == true)
            {
                AssemblyManager.CleanupOldVersionedAssemblies(logicalPath, keepCount: 3);
            }

            // Notify C++ that reload is complete
            // C++ will clear all entities and re-register systems to sync with updated component schema
            Logging.LogInternal($"[ScriptHost] Hot reload complete - C++ will clear entities and reinitialize systems", LogLevel.Info);

            // Prefer notifying with the actual loaded file path (important when original path doesn't exist and we load a versioned DLL).
            NotifyHotReloadComplete(newAssembly?.Location ?? assemblyPath);

            Logging.LogInternal($"[ScriptHost] Successfully reloaded: {assemblyPath}", LogLevel.Info);
            return 0; // Success
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in ReloadAssemblyInternal: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Compile all .cs files in a directory into an assembly using Roslyn.
    /// Called from C++ to compile scripts in-editor.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int CompileScriptsInDirectory(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
    {
        try
        {
            var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
            var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

            Logging.LogInternal($"[ScriptHost] CompileScriptsInDirectory: {dir} -> {outPath}", LogLevel.Info);

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            return res;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] CompileScriptsInDirectory error: {ex}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Compile directory and return pointer to UTF8 diagnostics string (allocated with CoTaskMemAlloc).
    /// Caller must free the returned pointer using FreeStringFromManaged.
    /// Returns IntPtr.Zero on failure to allocate.
    /// </summary>
    [UnmanagedCallersOnly]
    public static IntPtr CompileDirectoryWithDiagnostics(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
    {
        try
        {
            var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
            var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

            Logging.LogInternal($"[ScriptHost] CompileDirectoryWithDiagnostics: {dir} -> {outPath}", LogLevel.Info);

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);

            var diags = RoslynCompiler.GetLastDiagnostics() ?? string.Empty;
            // Encode as UTF8 and allocate unmanaged memory
            var bytes = System.Text.Encoding.UTF8.GetBytes(diags + '\0');
            IntPtr p = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, p, bytes.Length);

            return p;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] CompileDirectoryWithDiagnostics error: {ex}", LogLevel.Error);
            return IntPtr.Zero;
        }
    }

    [UnmanagedCallersOnly]
    public static void FreeStringFromManaged(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
            return;
        Marshal.FreeHGlobal(ptr);
    }

    /// <summary>
    /// Get the actual path to the last compiled assembly (may be versioned).
    /// Returns pointer to UTF8 string allocated with Marshal.AllocHGlobal.
    /// Returns IntPtr.Zero if no assembly has been compiled yet.
    /// Caller must free with FreeStringFromManaged.
    /// </summary>
    [UnmanagedCallersOnly]
    public static IntPtr GetLastCompiledAssemblyPath()
    {
        try
        {
            var path = RoslynCompiler.GetLastCompiledAssemblyPath();
            if (string.IsNullOrEmpty(path))
                return IntPtr.Zero;

            var bytes = System.Text.Encoding.UTF8.GetBytes(path + '\0');
            IntPtr p = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, p, bytes.Length);
            return p;
        }
        catch
        {
            return IntPtr.Zero;
        }
    }

    /// <summary>
    /// Compile scripts and reload resulting assembly.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int CompileAndReload(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
    {
        try
        {
            var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
            var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

            Logging.LogInternal($"[ScriptHost] CompileAndReload: {dir} -> {outPath}", LogLevel.Info);

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Logging.LogInternal("[ScriptHost] Compilation failed, aborting reload.", LogLevel.Error);
                return -1;
            }

            // Reload the compiled assembly
            return ReloadAssemblyInternal(outPath);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] CompileAndReload error: {ex}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Generate a minimal .csproj file in the specified directory to help external IDEs
    /// (VS / Rider) open the script folder. Writes to <dir>/<projectName>.csproj.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int GenerateCsProj(IntPtr outputDirPtr, IntPtr scriptsDirPtr, IntPtr projectNamePtr)
    {
        try
        {
            var outputDir = Marshal.PtrToStringUTF8(outputDirPtr) ?? string.Empty;
            var scriptsDir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
            var projectName = Marshal.PtrToStringUTF8(projectNamePtr) ?? string.Empty;

            if (string.IsNullOrWhiteSpace(projectName))
            {
                Logging.LogInternal($"[ScriptHost] GenerateCsProj: invalid project name!", LogLevel.Error);
                return -1;
            }

            if (string.IsNullOrWhiteSpace(outputDir) || !Directory.Exists(outputDir))
            {
                Logging.LogInternal($"[ScriptHost] GenerateCsProj: invalid output dir {outputDir}", LogLevel.Warning);
                return -1;
            }

            if (string.IsNullOrWhiteSpace(scriptsDir) || !Directory.Exists(scriptsDir))
            {
                Logging.LogInternal($"[ScriptHost] GenerateCsProj: invalid scripts dir {scriptsDir}", LogLevel.Warning);
                return -1;
            }

            string outPath = Path.Combine(outputDir, projectName + ".csproj");
            // Normalize the scripts directory path for the Include element
            string normalizedScriptsDir = Path.GetFullPath(scriptsDir);

            // .csproj template
            // Needs to:
            // - Target net9.0
            // - Include all .cs files in the scripts directory
            // - Reference GrapeEngine.Scripting for intellisense
            // - Reference GameScripts.dll for compiled types intellisense
            // - Exclude obj/ subdirectories
            string template = 
            $@"<Project Sdk=""Microsoft.NET.Sdk""> 
                <PropertyGroup>
                    <TargetFramework>net9.0</TargetFramework>
                    <ImplicitUsings>enable</ImplicitUsings>
                    <Nullable>enable</Nullable>
                </PropertyGroup>
                <ItemGroup>
                    <Compile Include=""{normalizedScriptsDir}\**\*.cs"" />
                </ItemGroup>
                <ItemGroup>
                    <Reference Include=""GrapeEngine.Scripting"">
                        <HintPath>..\GrapeEngine.Scripting.dll</HintPath>
                    </Reference>
                </ItemGroup>
                <ItemGroup>
                    <Compile Remove=""{normalizedScriptsDir}\obj\**"" />
                </ItemGroup>
            </Project>
                ";

            File.WriteAllText(outPath, template);
            Logging.LogInternal($"[ScriptHost] Generated csproj: {outPath}", LogLevel.Info);
            return 0;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] GenerateCsProj error: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    private static IntPtr StringToHGlobalUtf8(string? s)
    {
        if (s == null)
            return IntPtr.Zero;

        // Encode as UTF8 and allocate unmanaged memory
        var bytes = System.Text.Encoding.UTF8.GetBytes(s + '\0');
        IntPtr p = Marshal.AllocHGlobal(bytes.Length); // +1 for null terminator
        Marshal.Copy(bytes, 0, p, bytes.Length); // copy including null terminator

        return p;
    }

    /// <summary>
    /// Map an HRESULT to the runtime's managed Exception representation and
    /// return a UTF8 pointer describing the exception type and message.
    /// Caller must free the returned pointer with FreeStringFromManaged.
    /// </summary>
    [UnmanagedCallersOnly]
    public static IntPtr GetManagedExceptionForHResult(int hr)
    {
        try
        {
            // Get the managed exception for the HRESULT
            Exception? ex = Marshal.GetExceptionForHR(hr);
            string s = ex == null 
                ? "(no managed mapping)" 
                : $"{ex.GetType().FullName}: {ex.Message}";

            // Allocate unmanaged UTF8 string
            return StringToHGlobalUtf8(s);
        }
        catch (Exception e)
        {
            return StringToHGlobalUtf8($"GetExceptionForHR threw: {e.GetType().FullName}: {e.Message}");
        }
    }

    /// <summary>
    /// Return the number of diagnostics produced by the last Roslyn compilation.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int GetLastDiagnosticsCount()
    {
        try
        {
            return RoslynCompiler.GetLastDiagnosticsCount();
        }
        catch (Exception)
        {
            return 0;
        }
    }

    /// <summary>
    /// Return the diagnostic message at the specified index as an unmanaged UTF8 string.
    /// Caller must free the pointer with FreeStringFromManaged.
    /// </summary>
    [UnmanagedCallersOnly]
    public static IntPtr GetLastDiagnosticAt(int index)
    {
        try
        {
            string? s = RoslynCompiler.GetLastDiagnosticAt(index);
            return StringToHGlobalUtf8(s ?? string.Empty);
        }
        catch (Exception)
        {
            return IntPtr.Zero;
        }
    }

    /// <summary>
    /// Managed wrapper that compiles scripts in `dir` and reloads the resulting assembly at `outPath`.
    /// This is callable from other managed classes (e.g., file watcher).
    /// </summary>
    public static int TriggerCompileAndReloadManaged(string dir, string outPath)
    {
        try
        {
            // Unload the old assembly BEFORE compilation
            // This releases the DLL file lock so we can overwrite it
            Logging.LogInternal($"[ScriptHost] Unloading old assembly before recompilation: {outPath}", LogLevel.Info);
            if (!AssemblyManager.UnloadAssembly(outPath))
            {
                Logging.LogInternal($"[ScriptHost] Warning: Failed to unload existing assembly before compilation", LogLevel.Warning);
            }

            // CRITICAL: Wait for CLR to fully release the file lock
            // - GC.Collect() forces garbage collection (inside AssemblyManager.UnloadAssembly)
            // - GC.WaitForPendingFinalizers() waits for finalizers (inside AssemblyManager.UnloadAssembly)
            // - We do additional GC passes here to be extra sure
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect(); // Second pass to catch any objects freed by finalizers

            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Logging.LogInternal("[ScriptHost] Compilation failed in TriggerCompileAndReloadManaged", LogLevel.Error);
                return res;
            }

            // Get the actual compiled assembly path (will be versioned, e.g., GameScripts_hotreload_1.dll)
            string actualCompiledPath = RoslynCompiler.GetLastCompiledAssemblyPath();
            if (string.IsNullOrEmpty(actualCompiledPath))
            {
                Logging.LogInternal("[ScriptHost] Error: Compilation succeeded but no assembly path returned", LogLevel.Error);
                return -1;
            }

            Logging.LogInternal($"[ScriptHost] Compilation successful, reloading from: {actualCompiledPath}", LogLevel.Info);

            // Reload via the logical/original path so state saving + unloading happens in the correct order.
            // AssemblyManager.LoadAssembly(outPath) will resolve and load the latest versioned build.
            return ReloadAssemblyInternal(outPath);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] TriggerCompileAndReloadManaged error: {ex}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Discover all ISystem implementations in loaded assemblies.
    /// Called from C++ ScriptManager::DiscoverScriptedSystems()
    /// </summary>
    [UnmanagedCallersOnly]
    public static IntPtr DiscoverSystems(IntPtr outCountPtr)
    {
        try
        {
            Logging.LogInternal("[ScriptHost] Discovering systems in loaded assemblies...", LogLevel.Info);

            var allSystemHandles = new List<ulong>();

            // Discover systems in all loaded assemblies
            foreach (var assembly in AssemblyManager.GetAllLoadedAssemblies())
            {
                Logging.LogInternal($"[ScriptHost] Inspecting assembly: {assembly.FullName} (Location: {assembly.Location})", LogLevel.Info);
                string[] discovered = SystemDiscovery.DiscoverSystemsInAssembly(assembly);
                Logging.LogInternal($"[ScriptHost] DiscoverSystemsInAssembly returned {discovered.Length} entries for {assembly.FullName}", LogLevel.Info);
                foreach (var entry in discovered)
                {
                    // Parse "handle:typename" format (DiscoverSystemsInAssembly returns type handles)
                    var parts = entry.Split(':');
                    if (parts.Length == 2 && ulong.TryParse(parts[0], out ulong typeHandle))
                    {
                        // Ensure we create an instance for this type so native receives an INSTANCE handle
                        var instance = SystemDiscovery.CreateSystemInstance(typeHandle);
                        if (instance != null)
                        {
                            allSystemHandles.Add(typeHandle);
                        }
                        else
                        {
                            Logging.LogInternal($"[ScriptHost] Failed to create instance for discovered system handle={typeHandle}", LogLevel.Warning);
                        }
                    }
                }
            }

            Logging.LogInternal($"[ScriptHost] Found {allSystemHandles.Count} system types", LogLevel.Info);

            // Allocate unmanaged array for handles and write each 64-bit value
            int slotSize = Marshal.SizeOf<ulong>();
            IntPtr handlesPtr = Marshal.AllocHGlobal(slotSize * allSystemHandles.Count);
            for (var i = 0; i < allSystemHandles.Count; ++i)
            {
                IntPtr slot = IntPtr.Add(handlesPtr, i * slotSize);
                Marshal.WriteInt64(slot, unchecked((long)allSystemHandles[i]));
            }

            if (outCountPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outCountPtr, allSystemHandles.Count);
            }
            return handlesPtr;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error discovering systems: {ex.Message}", LogLevel.Error);
            if (outCountPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outCountPtr, 0);
            }
            return IntPtr.Zero;
        }
    }

    /// <summary>
    /// Create an instance of a scripted system.
    /// Called from C++ when creating ScriptSystemWrapper.
    /// </summary>
    [UnmanagedCallersOnly]
    public static ulong CreateSystemInstance(IntPtr typeNamePtr)
    {
        try
        {
            var typeName = Marshal.PtrToStringUTF8(typeNamePtr) ?? string.Empty;

            // Find the type in loaded assemblies
            Type? systemType = null;
            foreach (var assembly in AssemblyManager.GetAllLoadedAssemblies())
            {
                systemType = assembly.GetType(typeName);
                if (systemType != null)
                    break;
            }

            if (systemType == null)
            {
                Logging.LogInternal($"[ScriptHost] System type not found: {typeName}", LogLevel.Warning);
                return 0;
            }

            // Create instance and get handle
            ulong handle = SystemDiscovery.CreateSystemInstanceFromType(systemType);
            if (handle == 0)
            {
                Logging.LogInternal($"[ScriptHost] Failed to create instance of: {typeName}", LogLevel.Warning);
                return 0;
            }

            // If we have saved state from a previous unload, restore it
            StatePreserver.RestoreSystemState(systemType.Assembly.Location ?? string.Empty, SystemDiscovery.GetSystemInstance(handle)!);

            Logging.LogInternal($"[ScriptHost] Created system instance: {typeName} (handle: {handle})", LogLevel.Info);
            return handle;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error creating system instance: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }

    /// <summary>
    /// Destroy a system instance.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void DestroySystemInstance(ulong handle)
    {
        // Note: Instances remain in SystemDiscovery's dictionary until next reload
        // This is called to notify C++ that the managed system is being destroyed
    }

    /// <summary>
    /// Get metadata about a system (name, group, run mode).
    /// </summary>
    [UnmanagedCallersOnly]
    public static void GetSystemMetadata(ulong handle, IntPtr outNameBuffer, IntPtr outGroupPtr, IntPtr outRunModePtr)
    {
        try
        {
            Type? systemType = SystemDiscovery.GetSystemType(handle);
            if (systemType == null)
            {
                Logging.LogInternal($"[ScriptHost] System handle not found: {handle}", LogLevel.Warning);
                return;
            }
            
            // Get instance if available (for ISystemMetadata interface)
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            
            // Get name
            string name = systemType.FullName ?? systemType.Name;
            byte[] nameBytes = System.Text.Encoding.UTF8.GetBytes(name);
            if (outNameBuffer != IntPtr.Zero)
            {
                Marshal.Copy(nameBytes, 0, outNameBuffer, System.Math.Min(nameBytes.Length, 256));
            }
            
            // Get group - check ISystemMetadata interface first, then [SystemGroup] attribute
            if (outGroupPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outGroupPtr, (int)SystemMetadataExtractor.GetSystemGroup(systemType, instance));
            }
            
            // Get run mode - check for [ExecuteInEditMode] attribute
            if (outRunModePtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outRunModePtr, SystemMetadataExtractor.HasExecuteInEditMode(systemType) 
                    ? (int)SystemRunMode.Always 
                    : (int)SystemRunMode.PlayOnly);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error getting metadata: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Extract component access information from a scripted system for C++ dependency resolution.
    /// 
    /// Returns two arrays of component type hashes:
    /// 1. Read-only components (safe for parallel access)
    /// 2. Write-access components (exclusive access required)
    /// 
    /// Called from C++ during system registration to populate ComponentAccessBuilder metadata.
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "GetSystemComponentAccesses")]
    public static int GetSystemComponentAccesses(ulong handle, IntPtr outReadHashesPtr, IntPtr outWriteHashesPtr, int maxSize)
    {
        try
        {
            Type? systemType = SystemDiscovery.GetSystemType(handle);
            if (systemType == null)
            {
                Logging.LogInternal($"[ScriptHost] System handle not found: {handle}", LogLevel.Warning);
                return 0;
            }

            // Extract component accesses from C# attributes using ComponentAccessBridge
            var accesses = ComponentAccessBridge.ExtractComponentAccesses(systemType);
            
            // Separate into read and write accesses
            int readCount = 0;
            int writeCount = 0;
            var readHashes = new List<uint>();
            var writeHashes = new List<uint>();

            foreach (var (hash, mode) in accesses)
            {
                if (mode == ComponentAccessMode.Read)
                {
                    if (readCount < maxSize / 2)
                    {
                        readHashes.Add(hash);
                        readCount++;
                    }
                }
                else // Write or ReadWrite
                {
                    if (writeCount < maxSize / 2)
                    {
                        writeHashes.Add(hash);
                        writeCount++;
                    }
                }
            }
            
            // Copy arrays to unmanaged memory
            if (outReadHashesPtr != IntPtr.Zero && readCount > 0)
            {
                // Marshal.Copy does not have a uint[] overload, use int[] with identical bit-patterns
                int[] tmp = new int[readCount];
                for (int i = 0; i < readCount; ++i)
                    tmp[i] = unchecked((int)readHashes[i]);

                Marshal.Copy(tmp, 0, outReadHashesPtr, readCount);
            }
            if (outWriteHashesPtr != IntPtr.Zero && writeCount > 0)
            {
                int[] tmp = new int[writeCount];
                for (var i = 0; i < writeCount; ++i)
                    tmp[i] = unchecked((int)writeHashes[i]);

                Marshal.Copy(tmp, 0, outWriteHashesPtr, writeCount);
            }

            // Return total count (read count in lower 16 bits, write count in upper 16 bits)
            return (readCount) | (writeCount << 16);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error getting component accesses: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }

    /// <summary>
    /// Hash a component type name using FNV-1a algorithm in C++.
    /// Delegates to C++ to ensure hash consistency across language boundary.
    /// </summary>
    [UnmanagedCallersOnly]
    public static uint HashComponentTypeName(IntPtr typeNamePtr)
    {
        try
        {
            // For now, compute here but ideally this would call C++
            var typeName = Marshal.PtrToStringUTF8(typeNamePtr) ?? string.Empty;
            return ComponentAccessBridge.Fnv1aHashPublic(typeName);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error hashing component type: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }

    /// <summary>
    /// Resolve a system's execution group using the priority system in C++.
    /// 
    /// C# reflection extracts the [SystemGroup] attribute,
    /// C++ validates and applies priority resolution.
    /// 
    /// This keeps metadata priority logic centralized in C++ while C# handles reflection.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int ResolveSystemGroup(int attributeGroup)
    {
        try
        {
            // For now, return directly (C# already applied priority correctly)
            // In future: Could validate/transform the group with C++ logic
            return attributeGroup;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error resolving system group: {ex.Message}", LogLevel.Error);
            return 0; // Default to Update
        }
    }

    /// <summary>
    /// Call OnCreate on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnCreate(ulong handle, IntPtr worldPtr)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                return;
            }
            
            if (instance is ISystem system)
            {
                // Use cached World wrapper to avoid unnecessary allocations
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                system.OnCreate(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnCreate: {ex.Message}", LogLevel.Error);
        }
    }


    /// <summary>
    /// Call OnUpdate on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnUpdate(ulong handle, IntPtr worldPtr)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                return;
            }
            
            if (instance is ISystem system)
            {
                // Use cached World wrapper to avoid allocating a new object every frame
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                system.OnUpdate(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnUpdate: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnUpdate on a scripted system that may schedule jobs.
    /// Returns a native job handle (IntPtr) when the system implements ISystemJob
    /// and schedules work. Returns IntPtr.Zero otherwise.
    /// </summary>
    [UnmanagedCallersOnly]
    public static IntPtr CallSystemOnUpdateJob(ulong handle, IntPtr worldPtr, IntPtr dependsOnNativeHandle)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                return IntPtr.Zero;
            }

            if (instance is ISystemJob jobSystem)
            {
                // Use cached World wrapper to avoid allocating a new object every frame
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                JobHandle? depends = dependsOnNativeHandle == IntPtr.Zero ? null : new JobHandle(dependsOnNativeHandle);
                var result = jobSystem.OnUpdateWithJob(managedWorld, depends);
                return result != null ? new IntPtr(result.NativeHandle) : IntPtr.Zero;
            }
            else
            {
                // Fallback: call legacy OnUpdate
                if (instance is ISystem system)
                {
                    World managedWorld = new(worldPtr);
                    system.OnUpdate(managedWorld);
                }
                return IntPtr.Zero;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in CallSystemOnUpdateJob: {ex.Message}", LogLevel.Error);
            return IntPtr.Zero;
        }
    }
    /// <summary>
    /// Call OnDestroy on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnDestroy(ulong handle, IntPtr worldPtr)
    {
        try
        {
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                return;
            }
            
            if (instance is ISystem system)
            {
                // Use cached World wrapper to avoid unnecessary allocations
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                system.OnDestroy(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnDestroy: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Flush all buffered logs to the native side.
    /// Call this at the end of each frame to ensure all logs are delivered.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void FlushLogs()
    {
        try
        {
            Logging.Flush();
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in FlushLogs: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Deserialize component data from JSON and apply to component in memory.
    /// Called from the editor when component properties are modified at runtime.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void DeserializeComponentFromJson(uint typeHash, IntPtr componentPtr, int size, IntPtr jsonStrPtr)
    {
        try
        {
            if (componentPtr == IntPtr.Zero || size <= 0)
            {
                Logging.LogInternal("[ScriptHost] DeserializeComponentFromJson: Invalid component pointer or size", LogLevel.Warning);
                return;
            }

            var jsonStr = Marshal.PtrToStringUTF8(jsonStrPtr) ?? "{}";

            // Try to get the registered type for this hash
            if (!ComponentDiscovery.TypeHashToType.TryGetValue(typeHash, out var componentType))
            {
                Logging.LogInternal($"[ScriptHost] DeserializeComponentFromJson: No type registered for hash 0x{typeHash:X8}", LogLevel.Warning);
                return;
            }

            // Deserialize JSON to the component type using reflection
            var options = new System.Text.Json.JsonSerializerOptions 
            { 
                PropertyNameCaseInsensitive = true,
                // Allow reading properties that match fields
                IncludeFields = true
            };
            
            var deserializedObj = System.Text.Json.JsonSerializer.Deserialize(jsonStr, componentType, options);

            if (deserializedObj == null)
            {
                Logging.LogInternal($"[ScriptHost] DeserializeComponentFromJson: Failed to deserialize type {componentType.Name}", LogLevel.Warning);
                return;
            }

            // Copy the deserialized object back to the native component memory
            // This works for both mutable structs and record structs
            Marshal.StructureToPtr(deserializedObj, componentPtr, false);
            // Logging.LogInternal($"[ScriptHost] Applied component {componentType.Name} from JSON", LogLevel.Info);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in DeserializeComponentFromJson: {ex.Message}", LogLevel.Error);
        }
    }
}

// ============================================================================
// SCRIPTING ENUMS - Must match C++ Engine definitions
// ============================================================================
// These enums are marshaled between C# and C++ and MUST have identical values.
// Validation: See EnumParity validator below
// 
// C++ Source: engine/core/ecs/SystemGroup.h
// C# Validation: ScriptHost.ValidateEnumParity()
// ============================================================================

/// <summary>
/// System execution group - defines when systems run relative to engine lifecycle.
/// 
/// MUST match C++ ECS::SystemGroup enum values for correct P/Invoke marshaling.
/// If C++ values change, update both enums AND the validation logic below.
/// </summary>
public enum SystemGroup
{
    /// <summary>
    /// Systems execute before main update cycle (frame setup, input processing)
    /// </summary>
    PreUpdate = 0,
    
    /// <summary>
    /// Main update systems (gameplay logic, AI, etc.)
    /// </summary>
    Update = 1,
    
    /// <summary>
    /// Systems execute after update (cleanup, state finalization)
    /// </summary>
    PostUpdate = 2,
    
    /// <summary>
    /// Physics pre-calculation phase
    /// </summary>
    PrePhysics = 3,
    
    /// <summary>
    /// Physics simulation systems
    /// </summary>
    Physics = 4,
    
    /// <summary>
    /// Physics post-calculation phase (resolution, callbacks)
    /// </summary>
    PostPhysics = 5,
    
    /// <summary>
    /// Rendering preparation phase
    /// </summary>
    PreRender = 6,
    
    /// <summary>
    /// Render systems (camera updates, draw calls)
    /// </summary>
    Render = 7,
    
    /// <summary>
    /// Post-render cleanup (frame finalization)
    /// </summary>
    PostRender = 8
}

/// <summary>
/// System execution mode - determines when systems are active.
/// 
/// MUST match C++ ECS::SystemRunMode enum values for correct P/Invoke marshaling.
/// If C++ values change, update both enums AND the validation logic below.
/// </summary>
public enum SystemRunMode
{
    /// <summary>
    /// System always runs (edit and play mode)
    /// </summary>
    Always = 0,
    
    /// <summary>
    /// System runs only in play mode
    /// </summary>
    PlayOnly = 1,
    
    /// <summary>
    /// System runs only in editor/edit mode
    /// </summary>
    EditOnly = 2
}

/// <summary>
/// Validates that C# enum values match C++ engine definitions.
/// Called on assembly load to catch enum mismatches early.
/// </summary>
public static class EnumParityValidator
{
    /// <summary>
    /// Validate that all C# enums match C++ values.
    /// Should be called during assembly initialization.
    /// </summary>
    /// <returns>True if all enums match, false if mismatch detected</returns>
    public static bool ValidateEnumParity()
    {
        bool valid = true;

        // Validate SystemGroup enum
        valid &= ValidateSystemGroupEnum();
        
        // Validate SystemRunMode enum
        valid &= ValidateSystemRunModeEnum();

        if (!valid)
        {
            Logging.LogInternal("[EnumParityValidator] WARNING: Enum mismatch detected! " +
                "C# enum values do not match C++ definitions. This will cause P/Invoke marshaling errors.", LogLevel.Warning);
        }

        return valid;
    }

    /// <summary>
    /// Validate SystemGroup enum values against expected C++ values.
    /// </summary>
    private static bool ValidateSystemGroupEnum()
    {
        // Expected C++ values (must match engine/core/ecs/SystemGroup.h)
        var expectedValues = new Dictionary<SystemGroup, int>
        {
            { SystemGroup.PreUpdate, 0 },
            { SystemGroup.Update, 1 },
            { SystemGroup.PostUpdate, 2 },
            { SystemGroup.PrePhysics, 3 },
            { SystemGroup.Physics, 4 },
            { SystemGroup.PostPhysics, 5 },
            { SystemGroup.PreRender, 6 },
            { SystemGroup.Render, 7 },
            { SystemGroup.PostRender, 8 }
        };

        bool valid = true;
        foreach (var (group, expectedValue) in expectedValues)
        {
            int actualValue = (int)group;
            if (actualValue != expectedValue)
            {
                Logging.LogInternal(
                    $"[EnumParityValidator] SystemGroup.{group} mismatch: " +
                    $"expected {expectedValue}, got {actualValue}", LogLevel.Warning);
                valid = false;
            }
        }

        return valid;
    }

    /// <summary>
    /// Validate SystemRunMode enum values against expected C++ values.
    /// </summary>
    private static bool ValidateSystemRunModeEnum()
    {
        // Expected C++ values (must match engine/core/ecs/SystemRunMode.h)
        var expectedValues = new Dictionary<SystemRunMode, int>
        {
            { SystemRunMode.Always, 0 },
            { SystemRunMode.PlayOnly, 1 },
            { SystemRunMode.EditOnly, 2 }
        };

        bool valid = true;
        foreach (var (mode, expectedValue) in expectedValues)
        {
            int actualValue = (int)mode;
            if (actualValue != expectedValue)
            {
                Logging.LogInternal(
                    $"[EnumParityValidator] SystemRunMode.{mode} mismatch: " +
                    $"expected {expectedValue}, got {actualValue}", LogLevel.Warning);
                valid = false;
            }
        }

        return valid;
    }

    /// <summary>
    /// Get a detailed report of all enum values for documentation purposes.
    /// Useful for verifying against C++ source files.
    /// </summary>
    public static string GetEnumParityReport()
    {
        var report = new System.Text.StringBuilder();
        report.AppendLine("=== C# Enum Parity Report ===");
        report.AppendLine();

        report.AppendLine("SystemGroup values:");
        foreach (SystemGroup group in Enum.GetValues<SystemGroup>())
        {
            report.AppendLine($"  {group} = {(int)group}");
        }
        report.AppendLine();

        report.AppendLine("SystemRunMode values:");
        foreach (SystemRunMode mode in Enum.GetValues<SystemRunMode>())
        {
            report.AppendLine($"  {mode} = {(int)mode}");
        }

        return report.ToString();
    }
}
