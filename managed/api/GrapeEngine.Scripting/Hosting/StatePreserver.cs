/* Start Header *****************************************************************/
/*!
\file   StatePreserver.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Manages system state serialization for hot reload.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// StatePreserver - Encapsulates hot reload state preservation logic.
/// 
/// Responsibilities:
/// - Save system state before unload
/// - Restore system state after reload
/// - Track serialized state by assembly and type
/// 
/// HOW HOT RELOAD STATE WORKS:
/// 1. Before unloading old assembly: Serialize IHotReloadable systems
/// 2. Load new assembly with recompiled code
/// 3. After loading: Deserialize state into new system instances
/// 4. System continues with preserved data (no loss of runtime state)
/// </summary>
internal static class StatePreserver
{
    /// <summary>
    /// Saved serialized state for hot-reload.
    /// Key: Assembly path
    /// Value: Dictionary mapping type full name to serialized blob (null if serialization failed)
    /// </summary>
    private static readonly Dictionary<string, Dictionary<string, byte[]?>> _savedSystemStateByAssemblyPath = [];

    private static string NormalizeAssemblyKey(string assemblyPath)
    {
        if (string.IsNullOrWhiteSpace(assemblyPath))
            return assemblyPath;

        try
        {
            assemblyPath = Path.GetFullPath(assemblyPath);
        }
        catch
        {
            // Best-effort normalization; keep original string.
        }

        string dir = Path.GetDirectoryName(assemblyPath) ?? "";
        string filename = Path.GetFileNameWithoutExtension(assemblyPath);
        string ext = Path.GetExtension(assemblyPath);

        int hotreloadIndex = filename.LastIndexOf("_hotreload_", StringComparison.OrdinalIgnoreCase);
        if (hotreloadIndex > 0)
        {
            filename = filename.Substring(0, hotreloadIndex);
        }

        return Path.Combine(dir, filename + ext);
    }

    /// <summary>
    /// Save state from all IHotReloadable systems before unload.
    /// 
    /// Iterates through all instantiated systems and calls OnBeforeUnload()
    /// on those implementing IHotReloadable.
    /// </summary>
    /// <param name="assemblyPath">Path of assembly being unloaded (for tracking purposes)</param>
    public static void SaveAllSystemStates(string assemblyPath)
    {
        try
        {
            assemblyPath = NormalizeAssemblyKey(assemblyPath);
            var stateDict = new Dictionary<string, byte[]?>();

            foreach (var (handle, instance) in SystemDiscovery.GetAllSystemInstances())
            {
                if (instance is IHotReloadable hotReloadable)
                {
                    string typeName = instance.GetType().FullName ?? "Unknown";
                
                    try
                    {
                        byte[]? state = hotReloadable.OnBeforeUnload();
                        stateDict[typeName] = state;
                    
                        Logging.LogInternal($"[StatePreserver] Saved state for {typeName} ({state?.Length ?? 0} bytes)", LogLevel.Info);
                    }
                    catch (Exception ex)
                    {
                        Logging.LogInternal($"[StatePreserver] Error saving state for {typeName}: {ex.Message}", LogLevel.Error);
                        stateDict[typeName] = null;
                    }
                }
            }

            if (stateDict.Count > 0)
            {
                _savedSystemStateByAssemblyPath[assemblyPath] = stateDict;
                Logging.LogInternal($"[StatePreserver] Saved state for {stateDict.Count} systems from {assemblyPath}", LogLevel.Info);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[StatePreserver] Error in SaveAllSystemStates: {ex.Message}", LogLevel.Error);
        }

    }
    
    /// <summary>
    /// Restore state to a system instance after reload.
    /// 
    /// Looks up previously saved state and calls OnAfterHotReload()
    /// on systems implementing IHotReloadable.
    /// </summary>
    /// <param name="assemblyPath">Path of assembly that was reloaded</param>
    /// <param name="instance">System instance to restore state to</param>
    public static void RestoreSystemState(string assemblyPath, object instance)
    {
        try
        {
            assemblyPath = NormalizeAssemblyKey(assemblyPath);
            if (instance is not IHotReloadable hotReloadable)
            {
                return; // Not a hot-reloadable system
            }

            if (!_savedSystemStateByAssemblyPath.TryGetValue(assemblyPath, out var stateDict))
            {
                Logging.LogInternal($"[StatePreserver] No saved state found for assembly: {assemblyPath}", LogLevel.Warning);
                return;
            }

            string typeName = instance.GetType().FullName ?? "Unknown";
            
            if (!stateDict.TryGetValue(typeName, out var savedState))
            {
                Logging.LogInternal($"[StatePreserver] No saved state found for type: {typeName}", LogLevel.Warning);
                return;
            }

            if (savedState == null)
            {
                Logging.LogInternal($"[StatePreserver] Saved state was null for {typeName}", LogLevel.Warning);
                return;
            }

            try
            {
                hotReloadable.OnAfterReload(savedState);
                Logging.LogInternal($"[StatePreserver] Restored state for {typeName} ({savedState.Length} bytes)", LogLevel.Info);
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[StatePreserver] Error restoring state for {typeName}: {ex.Message}", LogLevel.Error);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[StatePreserver] Error in RestoreSystemState: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Clear all saved state.
    /// Used after successful reload to free memory.
    /// </summary>
    public static void ClearSavedState(string? assemblyPath = null)
    {
        if (assemblyPath != null)
        {
            _savedSystemStateByAssemblyPath.Remove(NormalizeAssemblyKey(assemblyPath));
        }
        else
        {
            _savedSystemStateByAssemblyPath.Clear();
        }
    }

    /// <summary>
    /// Clear all saved state from all assemblies.
    /// Used before unload to break any remaining references.
    /// </summary>
    public static void ClearAllSavedState()
    {
        _savedSystemStateByAssemblyPath.Clear();
        Logging.LogInternal($"[StatePreserver] Cleared all saved state", LogLevel.Info);
    }
}
