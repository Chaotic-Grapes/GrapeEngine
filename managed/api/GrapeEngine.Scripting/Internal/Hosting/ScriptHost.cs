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

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Core.Dependencies;
using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace GrapeEngine.Scripting.Internal.Hosting;

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
/// 
/// This class serves as the coordinator and C++ interface layer only.
/// </summary>
public static class ScriptHost
{
    private readonly record struct ManagedComponentSchema(uint Hash, int Size, string Signature, string Name);
    private readonly record struct ManagedComponentSnapshot(ulong EntityId, string Json);
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
    /// Delegate type for removing a component from all entities by component type hash.
    /// </summary>
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void NativeRemoveComponentCallback(uint typeHash);

    /// <summary>
    /// Callback function pointer for removing components (set by native code during initialization).
    /// </summary>
    private static IntPtr _nativeRemoveComponentCallback = IntPtr.Zero;

    /// <summary>
    /// Called from C++ to register the native RemoveComponentFromAllEntities callback.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void RegisterRemoveComponentCallback(IntPtr callbackPtr)
    {
        _nativeRemoveComponentCallback = callbackPtr;
        Logging.LogInternal("[ScriptHost] Remove component callback registered", LogLevel.Info);
    }

    /// <summary>
    /// Callback function pointer passed from C++ native code.
    /// </summary>
    private static IntPtr _nativeHotReloadCallback = IntPtr.Zero;

    /// <summary>
    /// Cache of World wrapper objects to avoid allocating new World objects every frame.
    /// Maps native world pointer to managed World wrapper.
    /// </summary>
    private static readonly Dictionary<IntPtr, World> _worldCache = [];
    private static readonly Dictionary<ulong, uint[]> _requireForUpdateCache = [];

    /// <summary>
    /// Track the currently active world for hot reload operations.
    /// This is set before hot reload so we can clear components from the active world during unload.
    /// </summary>
    private static IntPtr _currentWorldPtr = IntPtr.Zero;
    private static IntPtr _reconcileWorldPtr = IntPtr.Zero;
    private static Dictionary<uint, ManagedComponentSchema> _preUnloadManagedSchemas = new();
    private static Dictionary<uint, List<ManagedComponentSnapshot>> _preUnloadComponentSnapshots = new();
    private static bool _hasPendingSchemaReconcile = false;

    /// <summary>
    /// Set the current active world pointer for hot reload operations.
    /// Call this before triggering hot reload so UnloadAssembly knows which world to clear components from.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void SetCurrentWorldForHotReload(IntPtr worldPtr)
    {
        _currentWorldPtr = worldPtr;
        Logging.LogInternal($"[ScriptHost] Set current world for hot reload: {worldPtr:X16}", LogLevel.Info);
    }

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
    /// Clear the World wrapper cache. Called before assembly unload to break references
    /// that would prevent the AssemblyLoadContext from being garbage collected.
    /// 
    /// This is CRITICAL for hot reload: World instances hold strong references to
    /// types from the loaded assembly, preventing GC of the LoadContext.
    /// </summary>
    private static void ClearWorldCache()
    {
        try
        {
            int count = _worldCache.Count;
            _worldCache.Clear();
            if (count > 0)
            {
                Logging.LogInternal($"[ScriptHost] Cleared {count} cached World wrappers before assembly unload", LogLevel.Info);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error clearing World cache: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Clear cached exception references.
    /// Exception stack traces hold references to types from loaded assemblies,
    /// preventing garbage collection of the AssemblyLoadContext.
    /// This method clears the current thread's cached exception state.
    /// </summary>
    /// <summary>
    /// Clear cached exception references by triggering garbage collection.
    /// Exception stack traces hold references to types from loaded assemblies,
    /// preventing garbage collection of the AssemblyLoadContext.
    /// This method encourages the GC to clean up exception objects.
    /// </summary>
    private static void ClearCachedExceptions()
    {
        try
        {
            // Trigger garbage collection to clean up any cached exception objects
            // Exception objects hold stack trace references to assembly types
            GC.Collect(0);
            GC.WaitForPendingFinalizers();
            Logging.LogInternal($"[ScriptHost] Triggered GC to clear cached exception references", LogLevel.Debug);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error during exception cleanup: {ex.Message}", LogLevel.Debug);
        }
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
                ReconcileManagedComponentsAfterReload();
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
            
            // Capture current managed component schemas before cache clear.
            // We will remove only deleted/changed components after the new assembly is loaded.
            _preUnloadManagedSchemas = CaptureManagedComponentSchemas();
            _reconcileWorldPtr = _currentWorldPtr;
            _preUnloadComponentSnapshots = CaptureManagedComponentSnapshots(_reconcileWorldPtr, _preUnloadManagedSchemas.Keys);
            _hasPendingSchemaReconcile = true;
            _currentWorldPtr = IntPtr.Zero;
            
            // Clear World wrapper cache (holds instances that reference assembly types)
            ClearWorldCache();
            
            // Clear cached exception state to break stack trace references to assembly types
            ClearCachedExceptions();

            // Clear the type hash cache (holds Type objects from the assembly)
            // This is CRITICAL - types stay alive in this cache preventing GC of AssemblyLoadContext
            ComponentTypeHelper.ClearTypeHashCache();

            // Clear all registered component types from serializer (holds Type mappings)
            ComponentSerializer.ClearAllRegisteredTypes();
            
            // Clear the component registry's hash set (allows re-registration of modified types)
            ComponentRegistry.ClearRegistrationCache();
            
            // Clear all component discovery cache (holds Type -> Hash mappings)
            ComponentDiscovery.ClearTypeCache();

            // Clear all discovered systems (disposes IDisposable instances)
            // This breaks references from instances back to assembly types
            SystemDiscovery.ClearDiscoveredSystems();
            ClearRequireForUpdateCache();
            
            // Force additional GC to clean up any dangling references before AssemblyManager unload
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();

            // Unload assembly
            bool success = AssemblyManager.UnloadAssembly(assemblyPath);
            if (!success)
            {
                Logging.LogInternal($"[ScriptHost] AssemblyManager.UnloadAssembly returned false for: {assemblyPath}", LogLevel.Error);
                return -1;
            }

            Logging.LogInternal($"[ScriptHost] Successfully unloaded: {assemblyPath}", LogLevel.Info);
            
            // Check if assembly is still in loaded assemblies (diagnostic)
            bool stillLoaded = AssemblyManager.IsAssemblyLoaded(assemblyPath);
            if (stillLoaded)
            {
                Logging.LogInternal($"[ScriptHost] WARNING: Assembly still in loaded assemblies after unload: {assemblyPath}", LogLevel.Warning);
            }
            else
            {
                Logging.LogInternal($"[ScriptHost] Confirmed: Assembly removed from loaded assemblies: {assemblyPath}", LogLevel.Info);
            }
            
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
    /// C++ orchestrates the timing:
    /// 1. C++ compiles (via CompileScriptsManaged)
    /// 2. C++ unloads old assembly (via UnloadAssembly)  
    /// 3. C++ moves temp->final assembly location
    /// 4. C++ calls ReloadAssembly to load new version
    /// 5. C++ calls C# post-reload hook (re-discovers systems, etc.)
    /// 6. C++ resynchronizes game state
    /// 
    /// This function just loads the assembly and re-discovers components/systems.
    /// </summary>
    private static int ReloadAssemblyInternal(string assemblyPath)
    {
        try
        {
            string logicalPath = ExtractOriginalPathFromVersioned(assemblyPath);

            Logging.LogInternal($"[ScriptHost] ReloadAssemblyInternal: {assemblyPath} (logical: {logicalPath})", LogLevel.Info);

            // Load new version
            if (AssemblyManager.LoadAssembly(assemblyPath) == null)
            {
                Logging.LogInternal($"[ScriptHost] Failed to load new assembly during reload", LogLevel.Error);
                return -1;
            }

            // Re-discover and register all components in loaded assemblies
            // This is CRITICAL for hot reload - C++ needs updated component definitions
            ComponentDiscovery.DiscoverAndRegisterAll();
            ReconcileManagedComponentsAfterReload();
            Logging.LogInternal($"[ScriptHost] Re-discovered components after reload", LogLevel.Info);

            // Clear discovered systems before discovering new ones to avoid duplication
            SystemDiscovery.ClearDiscoveredSystems();
            ClearRequireForUpdateCache();

            // Discover systems in newly loaded assembly
            Assembly? newAssembly = AssemblyManager.GetLoadedAssembly(logicalPath);
            if (newAssembly != null)
            {
                SystemDiscovery.DiscoverSystemsInAssembly(newAssembly);
            }

            // Clean up old versioned assemblies if this was a versioned load
            if (newAssembly?.Location?.Contains("_hotreload_", StringComparison.OrdinalIgnoreCase) == true)
            {
                AssemblyManager.CleanupOldVersionedAssemblies(logicalPath, keepCount: 3);
            }

            // Notify C++ that reload is complete
            // C++ will:
            // 1. Remove all C# components from entities  
            // 2. Unregister old scripted systems
            // 3. Re-register new scripted systems (will call OnCreate)
            // 4. Resynchronize game state
            Logging.LogInternal($"[ScriptHost] Assembly loaded and systems discovered - notifying C++ to sync", LogLevel.Info);

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
            var path = RoslynCompiler.GetLastCompiledTempAssemblyPath();
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
    /// Compile scripts in directory (for initial editor compilation).
    /// Diagnostics-only version - returns compilation results without orchestrating reload.
    /// C++ controls all assembly lifecycle (unload, load, move).
    /// </summary>
    [UnmanagedCallersOnly]
    public static int CompileAndReload(IntPtr scriptsDirPtr, IntPtr outputAssemblyPathPtr)
    {
        try
        {
            var dir = Marshal.PtrToStringUTF8(scriptsDirPtr) ?? string.Empty;
            var outPath = Marshal.PtrToStringUTF8(outputAssemblyPathPtr) ?? string.Empty;

            Logging.LogInternal($"[ScriptHost] CompileAndReload: {dir} -> {outPath}", LogLevel.Info);

            // C# only compiles - writes to temp location
            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res != 0)
            {
                Logging.LogInternal("[ScriptHost] Compilation failed", LogLevel.Error);
                return -1;
            }

            // C++ will handle: unload -> move -> load -> re-discover systems
            // C# just reports success and provides temp path via GetLastCompiledTempAssemblyPath
            Logging.LogInternal($"[ScriptHost] Compilation succeeded - C++ will orchestrate assembly reload", LogLevel.Info);
            return 0;
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
    /// Force a full garbage collection cycle from native code.
    /// This is exposed to native via UnmanagedCallersOnly so the host
    /// can explicitly request GC at critical points (e.g. after unload).
    /// </summary>
    [UnmanagedCallersOnly]
    public static void ForceGarbageCollection()
    {
        try
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            Logging.LogInternal("[ScriptHost] ForceGarbageCollection invoked from native", LogLevel.Debug);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] ForceGarbageCollection error: {ex.Message}", LogLevel.Error);
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
    /// <summary>
    /// Compile scripts in a directory into an assembly.
    /// Called from C++ to perform Roslyn compilation on a background thread.
    /// Returns 0 on success, -1 on failure.
    /// 
    /// C++ owns orchestration - unload/load/reload is handled by C++.
    /// This function only compiles. The compiled assembly is written to a temp location.
    /// C++ moves it to the final location after it unloads the old assembly.
    /// </summary>
    public static int CompileScriptsManaged(string dir, string outPath)
    {
        try
        {
            Logging.LogInternal($"[ScriptHost] CompileScriptsManaged: {dir} -> {outPath}", LogLevel.Info);
            
            int res = RoslynCompiler.CompileDirectoryToAssembly(dir, outPath);
            if (res == 0)
            {
                Logging.LogInternal($"[ScriptHost] Compilation successful", LogLevel.Info);
            }
            else
            {
                Logging.LogInternal($"[ScriptHost] Compilation failed", LogLevel.Error);
            }
            return res;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] CompileScriptsManaged error: {ex.Message}", LogLevel.Error);
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
    /// Internal helper to clear all managed C# components from entities.
    /// This logic is shared by both the P/Invoke entry point and internal hot reload operations.
    /// </summary>
    private static void ClearAllManagedComponentsInternal()
    {
        try
        {
            Logging.LogInternal($"[ScriptHost] Clearing all managed C# components from entities", LogLevel.Info);
            
            // Get all discovered component types from ComponentDiscovery
            // These are the C# components that may exist on entities
            var allComponentTypes = ComponentDiscovery.TypeHashToType.Values.ToList();
            
            Logging.LogInternal($"[ScriptHost] Found {allComponentTypes.Count} managed component types to clear", LogLevel.Info);
            
            // For each component type, notify the native side to remove it from all entities
            if (_nativeRemoveComponentCallback != IntPtr.Zero)
            {
                foreach (var componentType in allComponentTypes)
                {
                    // Use SHORT NAME only (must match what ComponentTypeHelper uses during registration)
                    // ComponentTypeHelper.GetTypeHash uses type.Name, not type.FullName
                    string shortName = componentType.Name;
                    uint typeHash = ComponentAccessBridge.Fnv1aHashPublic(shortName);
                    try
                    {
                        // Invoke the native callback to remove this component type from all entities
                        // This iterates all entities in the world and removes the component if present
                        Logging.LogInternal($"[ScriptHost] Removing component type: {componentType.FullName} (name: {shortName}, hash: 0x{typeHash:X8})", LogLevel.Debug);
                        
                        var callback = Marshal.GetDelegateForFunctionPointer<NativeRemoveComponentCallback>(_nativeRemoveComponentCallback);
                        callback?.Invoke(typeHash);
                    }
                    catch (Exception ex)
                    {
                        Logging.LogInternal($"[ScriptHost] Warning: Failed to clear component {componentType.FullName}: {ex.Message}", LogLevel.Warning);
                    }
                }
            }
            else
            {
                Logging.LogInternal($"[ScriptHost] Native remove component callback not registered; skipping selective removal", LogLevel.Warning);
            }
            
            Logging.LogInternal($"[ScriptHost] Finished clearing managed components", LogLevel.Info);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error clearing managed components: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Called from C++ before hot reload to clear all C# component instances from entities.
    /// 
    /// This is necessary because:
    /// 1. When scripts are recompiled, component type definitions may change (fields added/removed/modified)
    /// 2. Existing entity instances have the OLD binary layout in their component storage
    /// 3. We must remove old component instances so the reload can register updated component definitions
    /// 4. New components will be deserialized with the updated schema
    /// 
    /// Called before ReloadAssembly so that UnregisterScriptedSystems gets clean state.
    /// 
    /// NOTE: Do NOT clear the component registry cache here! It needs to remain available
    /// so that component type hashes can be found during removal. The cache will be cleared
    /// during UnloadAssembly after unload completes.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void ClearAllManagedComponentsFromEntities(IntPtr worldPtr)
    {
        try
        {
            // Call the internal implementation to remove components from entities
            ClearAllManagedComponentsInternal();
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in ClearAllManagedComponentsFromEntities: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
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
    public static void GetSystemMetadata(ulong handle, IntPtr outNameBuffer, IntPtr outGroupPtr, IntPtr outRunModePtr, IntPtr outOrderPtr)
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
                const int maxLen = 256;
                int copyLen = System.Math.Min(nameBytes.Length, maxLen - 1);
                if (copyLen > 0)
                {
                    Marshal.Copy(nameBytes, 0, outNameBuffer, copyLen);
                }
                Marshal.WriteByte(outNameBuffer, copyLen, 0);
            }
            
            // Get group from ISystemMetadata interface first, then [SystemGroup] attribute
            if (outGroupPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outGroupPtr, (int)SystemMetadataExtractor.GetSystemGroup(systemType, instance));
            }
            
            // Same as above, but for SystemRunMode
            if (outRunModePtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outRunModePtr, (int)SystemMetadataExtractor.GetSystemRunMode(systemType, instance));
            }

            if (outOrderPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(outOrderPtr, SystemMetadataExtractor.GetSystemOrder(systemType));
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

    // Clear the cached required component hashes for a system, called after assembly reload to ensure we re-extract updated metadata
    private static void ClearRequireForUpdateCache()
    {
        _requireForUpdateCache.Clear();
    }

    private static Dictionary<uint, ManagedComponentSchema> CaptureManagedComponentSchemas()
    {
        var schemas = new Dictionary<uint, ManagedComponentSchema>();

        foreach (var type in ComponentDiscovery.TypeHashToType.Values)
        {
            try
            {
                uint hash = ComponentTypeHelper.GetTypeHash(type);
                int size = Marshal.SizeOf(type);
                string signature = BuildComponentSignature(type);
                string name = type.FullName ?? type.Name;
                schemas[hash] = new ManagedComponentSchema(hash, size, signature, name);
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[ScriptHost] Failed to capture schema for {type.FullName}: {ex.Message}", LogLevel.Warning);
            }
        }

        return schemas;
    }

    private static unsafe Dictionary<uint, List<ManagedComponentSnapshot>> CaptureManagedComponentSnapshots(
        IntPtr worldPtr,
        IEnumerable<uint> componentHashes)
    {
        // Capture JSON snapshots keyed by component hash so changed schemas can be restored
        // onto surviving entities after assembly reload.
        var snapshots = new Dictionary<uint, List<ManagedComponentSnapshot>>();
        if (worldPtr == IntPtr.Zero)
            return snapshots;

        void* nativeWorldPtr = (void*)worldPtr;

        foreach (uint hash in componentHashes)
        {
            QueryIterator iterator = default;
            uint queryHash = hash;
            if (!QueryInteropAPI.CreateQuery(nativeWorldPtr, &queryHash, 1, null, 0, null, 0, &iterator))
                continue;

            List<ManagedComponentSnapshot> perHash = new();
            ulong entityId = 0;
            while (QueryInteropAPI.QueryNext(&iterator, &entityId))
            {
                // Serializer allocates UTF-8 memory on the native side; always free in finally.
                nint jsonPtr = WorldAPI.SerializeComponentToJson(nativeWorldPtr, entityId, hash);
                if (jsonPtr == IntPtr.Zero)
                    continue;

                try
                {
                    string json = Marshal.PtrToStringUTF8(jsonPtr) ?? "{}";
                    perHash.Add(new ManagedComponentSnapshot(entityId, json));
                }
                finally
                {
                    WorldAPI.FreeSerializedString(jsonPtr);
                }
            }

            if (perHash.Count > 0)
            {
                snapshots[hash] = perHash;
            }
        }

        return snapshots;
    }

    private static string BuildComponentSignature(Type componentType)
    {
        var props = componentType.GetProperties(BindingFlags.Public | BindingFlags.Instance)
            .Where(p => p.GetIndexParameters().Length == 0)
            .OrderBy(p => p.MetadataToken)
            .Select(p => $"P:{p.Name}:{p.PropertyType.FullName}");

        var fields = componentType.GetFields(BindingFlags.Public | BindingFlags.Instance)
            .Where(f => !f.IsStatic && !f.Name.Contains("k__BackingField", StringComparison.Ordinal))
            .OrderBy(f => f.MetadataToken)
            .Select(f => $"F:{f.Name}:{f.FieldType.FullName}");

        return string.Join("|", props.Concat(fields));
    }

    private static void ReconcileManagedComponentsAfterReload()
    {
        if (!_hasPendingSchemaReconcile)
            return;

        // Reconcile in two phases:
        // 1) remove deleted/shape-changed component hashes globally
        // 2) restore JSON snapshots only for shape-changed hashes
        if (_nativeRemoveComponentCallback == IntPtr.Zero)
        {
            Logging.LogInternal("[ScriptHost] Cannot reconcile managed components: remove callback not registered", LogLevel.Warning);
            _reconcileWorldPtr = IntPtr.Zero;
            _preUnloadComponentSnapshots.Clear();
            _hasPendingSchemaReconcile = false;
            _preUnloadManagedSchemas.Clear();
            return;
        }

        var newSchemas = CaptureManagedComponentSchemas();
        var hashesToRemove = new HashSet<uint>();
        var hashesToRestore = new HashSet<uint>();

        foreach (var (hash, oldSchema) in _preUnloadManagedSchemas)
        {
            if (!newSchemas.TryGetValue(hash, out var newSchema))
            {
                // Component removed from scripts: remove from all entities, do not restore.
                hashesToRemove.Add(hash);
                continue;
            }

            // Same hash but different layout: remove stale bytes, then restore via JSON.
            if (oldSchema.Size != newSchema.Size || !string.Equals(oldSchema.Signature, newSchema.Signature, StringComparison.Ordinal))
            {
                hashesToRemove.Add(hash);
                hashesToRestore.Add(hash);
            }
        }

        if (hashesToRemove.Count > 0)
        {
            var callback = Marshal.GetDelegateForFunctionPointer<NativeRemoveComponentCallback>(_nativeRemoveComponentCallback);
            foreach (uint hash in hashesToRemove)
            {
                try
                {
                    callback?.Invoke(hash);
                    Logging.LogInternal($"[ScriptHost] Removed outdated managed component hash 0x{hash:X8}", LogLevel.Info);
                }
                catch (Exception ex)
                {
                    Logging.LogInternal($"[ScriptHost] Failed removing managed component hash 0x{hash:X8}: {ex.Message}", LogLevel.Warning);
                }
            }
        }

        // Restore changed components on entities that previously had them.
        // Deleted components are intentionally not restored.
        if (_reconcileWorldPtr != IntPtr.Zero && hashesToRestore.Count > 0)
        {
            unsafe
            {
                void* nativeWorld = (void*)_reconcileWorldPtr;
                foreach (uint hash in hashesToRestore)
                {
                    if (!_preUnloadComponentSnapshots.TryGetValue(hash, out var entitySnapshots))
                        continue;
                    if (!newSchemas.TryGetValue(hash, out var schema))
                        continue;

                    foreach (var snap in entitySnapshots)
                    {
                        try
                        {
                            // Entity may have been destroyed during reload window.
                            if (!WorldAPI.IsEntityAlive(nativeWorld, snap.EntityId))
                                continue;

                            if (!WorldAPI.HasComponent(nativeWorld, snap.EntityId, hash))
                            {
                                // Add zeroed storage with the new size before JSON patching.
                                WorldAPI.AddComponent(nativeWorld, snap.EntityId, hash, null, schema.Size);
                            }

                            WorldAPI.DeserializeComponentFromJson(nativeWorld, snap.EntityId, hash, snap.Json);
                        }
                        catch (Exception ex)
                        {
                            Logging.LogInternal(
                                $"[ScriptHost] Failed restoring component hash 0x{hash:X8} on entity {snap.EntityId}: {ex.Message}",
                                LogLevel.Warning);
                        }
                    }
                }
            }
        }

        _reconcileWorldPtr = IntPtr.Zero;
        _preUnloadComponentSnapshots.Clear();
        _preUnloadManagedSchemas.Clear();
        _hasPendingSchemaReconcile = false;
    }

    // Get the required component hashes for update for a system, using a cache to avoid repeated reflection
    private static uint[] GetRequireForUpdateHashes(ulong handle)
    {
        // Check cache first to avoid reflection on every ShouldRun call
        if (_requireForUpdateCache.TryGetValue(handle, out var hashes))
        {
            return hashes;
        }

        // If not in cache, extract from system type and cache it
        Type? systemType = SystemDiscovery.GetSystemType(handle);
        if (systemType == null)
        {
            hashes = [];
            _requireForUpdateCache[handle] = hashes;
            return hashes;
        }

        hashes = ComponentAccessBridge.ExtractRequireForUpdateComponentHashes(systemType).ToArray();
        _requireForUpdateCache[handle] = hashes;
        return hashes;
    }

    private static unsafe bool MatchesRequireForUpdate(World world, uint[] requiredHashes)
    {
        if (requiredHashes.Length == 0)
        {
            return true;
        }

        // A system should run only if at least one entity satisfies RequireForUpdate.
        fixed (uint* reqPtr = requiredHashes)
        {
            QueryIterator iterator = default;
            QueryInteropAPI.CreateQuery(world.NativePtr, reqPtr, requiredHashes.Length, null, 0, null, 0, &iterator);
            ulong entityId = 0;
            return QueryInteropAPI.QueryNext(&iterator, &entityId);
        }
    }

    /// <summary>
    /// Call ShouldRun on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int CallSystemShouldRun(ulong handle, IntPtr worldPtr)
    {
        try
        {
            // Get the system instance for this handle
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                return 1;
            }

            // If the system implements ISystem, call ShouldRun and return the result
            if (instance is ISystem system)
            {
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                uint[] requiredHashes = GetRequireForUpdateHashes(handle);
                if (!MatchesRequireForUpdate(managedWorld, requiredHashes))
                {
                    return 0;
                }
                return system.ShouldRun(managedWorld) ? 1 : 0;
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in ShouldRun: {ex.Message}", LogLevel.Error);
        }

        return 1;
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
    /// Call OnSceneStart on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnSceneStart(ulong handle)
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
                system.OnSceneStart();
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnSceneStart: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnSceneStop on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnSceneStop(ulong handle)
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
                system.OnSceneStop();
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnSceneStop: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnStartRunning on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnStartRunning(ulong handle, IntPtr worldPtr)
    {
        try
        {
            // Get the system instance for this handle and call OnStartRunning if it implements ISystem
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                return;
            }

            if (instance is ISystem system)
            {
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                system.OnStartRunning(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnStartRunning: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Call OnStopRunning on a scripted system.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void CallSystemOnStopRunning(ulong handle, IntPtr worldPtr)
    {
        try
        {
            // Get the system instance for this handle and call OnStopRunning if it implements ISystem
            object? instance = SystemDiscovery.GetSystemInstance(handle);
            if (instance == null)
            {
                Logging.LogInternal($"[ScriptHost] System instance not found: {handle}", LogLevel.Warning);
                return;
            }

            if (instance is ISystem system)
            {
                World managedWorld = GetOrCreateWorldWrapper(worldPtr);
                system.OnStopRunning(managedWorld);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in OnStopRunning: {ex.Message}", LogLevel.Error);
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

            // Prefer serializer's byte-copy path to preserve blittable layout exactly.
            if (ComponentSerializer.TryDeserializeFromJson(typeHash, componentPtr, size, jsonStr))
            {
                return;
            }

            // Avoid direct System.Text.Json type-based deserialization here.
            // It can retain reflection-emit caches across collectible ALC unload.
            Logging.LogInternal(
                $"[ScriptHost] DeserializeComponentFromJson: No serializer mapping for hash 0x{typeHash:X8}",
                LogLevel.Warning);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptHost] Error in DeserializeComponentFromJson: {ex.Message}", LogLevel.Error);
        }
    }
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


