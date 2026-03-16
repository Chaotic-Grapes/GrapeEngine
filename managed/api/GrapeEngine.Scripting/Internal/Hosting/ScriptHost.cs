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

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// SCRIPT HOST - Main P/Invoke entry point for script hosting.
/// 
/// Delegates specialized tasks to focused helper classes:
/// - AssemblyManager: Assembly lifecycle (load/unload)
/// - SystemDiscovery: System discovery and instantiation
/// 
/// This class serves as the coordinator and C++ interface layer only.
/// </summary>
public static partial class ScriptHost
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
    /// Delegate type for removing a component from all entities by component type hash.
    /// </summary>
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void NativeRemoveComponentCallback(uint typeHash);

    /// <summary>
    /// Callback function pointer for removing components (set by native code during initialization).
    /// </summary>
    private static IntPtr _nativeRemoveComponentCallback = IntPtr.Zero;
    private static NativeRemoveComponentCallback? _nativeRemoveComponent;

    /// <summary>
    /// Called from C++ to register the native RemoveComponentFromAllEntities callback.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void RegisterRemoveComponentCallback(IntPtr callbackPtr)
    {
        lock (s_stateLock)
        {
            _nativeRemoveComponentCallback = callbackPtr;
            _nativeRemoveComponent = callbackPtr != IntPtr.Zero
                ? Marshal.GetDelegateForFunctionPointer<NativeRemoveComponentCallback>(callbackPtr)
                : null;
        }
        Logging.LogInternal("[ScriptHost] Remove component callback registered", LogLevel.Info);
    }

    /// <summary>
    /// Callback function pointer passed from C++ native code.
    /// </summary>
    private static IntPtr _nativeHotReloadCallback = IntPtr.Zero;
    private static NativeHotReloadCallback? _nativeHotReload;
    private static readonly object s_stateLock = new();

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

    private static bool IsRecoverableInteropException(Exception ex)
    {
        return ex is ArgumentException
            or InvalidOperationException
            or ObjectDisposedException
            or IOException
            or UnauthorizedAccessException
            or NotSupportedException
            or MarshalDirectiveException
            or ExternalException
            or TargetInvocationException
            or TypeLoadException
            or FileLoadException
            or BadImageFormatException;
    }

    /// <summary>
    /// Set the current active world pointer for hot reload operations.
    /// Call this before triggering hot reload so UnloadAssembly knows which world to clear components from.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void SetCurrentWorldForHotReload(IntPtr worldPtr)
    {
        lock (s_stateLock)
        {
            _currentWorldPtr = worldPtr;
        }
        Logging.LogInternal($"[ScriptHost] Set current world for hot reload: {worldPtr:X16}", LogLevel.Info);
    }

    /// <summary>
    /// Get or create a cached World wrapper for a native world pointer.
    /// Reuses the same managed World object across frames to minimize allocations.
    /// </summary>
    private static World GetOrCreateWorldWrapper(IntPtr worldPtr)
    {
        lock (s_stateLock)
        {
            if (_worldCache.TryGetValue(worldPtr, out World? cachedWorld))
            {
                return cachedWorld;
            }

            var newWorld = new World(worldPtr);
            _worldCache[worldPtr] = newWorld;
            return newWorld;
        }
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
            int count;
            lock (s_stateLock)
            {
                count = _worldCache.Count;
                _worldCache.Clear();
            }
            if (count > 0)
            {
                Logging.LogInternal($"[ScriptHost] Cleared {count} cached World wrappers before assembly unload", LogLevel.Info);
            }
        }
        catch (Exception ex) when (IsRecoverableInteropException(ex))
        {
            Logging.LogInternal($"[ScriptHost] Error clearing World cache: {ex.Message}", LogLevel.Error);
        }
    }

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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
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
        lock (s_stateLock)
        {
            _nativeHotReloadCallback = callbackPtr;
            _nativeHotReload = callbackPtr != IntPtr.Zero
                ? Marshal.GetDelegateForFunctionPointer<NativeHotReloadCallback>(callbackPtr)
                : null;
        }
        Logging.LogInternal("[ScriptHost] Hot reload callback registered", LogLevel.Info);
    }

    /// <summary>
    /// Notify C++ that hot reload is complete by invoking the native callback.
    /// </summary>
    private static void NotifyHotReloadComplete(string assemblyPath)
    {
        NativeHotReloadCallback? callback;
        lock (s_stateLock)
        {
            callback = _nativeHotReload;
        }

        if (callback == null)
        {
            return;
        }

        try
        {
            callback(assemblyPath);
        }
        catch (Exception ex) when (IsRecoverableInteropException(ex))
        {
            Logging.LogInternal($"[ScriptHost] Error invoking hot reload callback: {ex.Message}", LogLevel.Error);
        }
    }
    /// <summary>
    /// Called from C++ ScriptManager::LoadAssembly().
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
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
            lock (s_stateLock)
            {
                _reconcileWorldPtr = _currentWorldPtr;
                _currentWorldPtr = IntPtr.Zero;
            }
            _preUnloadComponentSnapshots = CaptureManagedComponentSnapshots(_reconcileWorldPtr, _preUnloadManagedSchemas.Keys);
            _hasPendingSchemaReconcile = true;
            
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
        {
            Logging.LogInternal($"[ScriptHost] Error in ReloadAssemblyInternal: {ex.Message}", LogLevel.Error);
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
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
            NativeRemoveComponentCallback? callback;
            lock (s_stateLock)
            {
                callback = _nativeRemoveComponent;
            }

            if (callback != null)
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
                        
                        callback(typeHash);
                    }
                    catch (Exception ex) when (IsRecoverableInteropException(ex))
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
        {
            Logging.LogInternal($"[ScriptHost] Error in ClearAllManagedComponentsFromEntities: {ex.Message}", LogLevel.Error);
        }
    }

    // Clear the cached required component hashes for a system, called after assembly reload to ensure we re-extract updated metadata

    // Get the required component hashes for update for a system, using a cache to avoid repeated reflection

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
        catch (Exception ex) when (IsRecoverableInteropException(ex))
        {
            Logging.LogInternal($"[ScriptHost] Error in DeserializeComponentFromJson: {ex.Message}", LogLevel.Error);
        }
    }
}
