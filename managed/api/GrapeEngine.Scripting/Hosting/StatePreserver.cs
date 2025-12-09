/* Start Header *****************************************************************/
/*!
\file   StatePreserver.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Manages system state serialization for hot reload.
*/
/* End Header *******************************************************************/

using System;
using System.Collections.Generic;
using GrapeEngine.Scripting.Hosting;

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
    private static readonly Dictionary<string, Dictionary<string, byte[]?>> s_savedSystemStateByAssemblyPath = new();

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
                    
                    Console.WriteLine($"[StatePreserver] Saved state for {typeName} ({state?.Length ?? 0} bytes)");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[StatePreserver] Error saving state for {typeName}: {ex.Message}");
                    stateDict[typeName] = null;
                }
            }
        }

        if (stateDict.Count > 0)
        {
            s_savedSystemStateByAssemblyPath[assemblyPath] = stateDict;
            Console.WriteLine($"[StatePreserver] Saved state for {stateDict.Count} systems from {assemblyPath}");
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[StatePreserver] Error in SaveAllSystemStates: {ex.Message}");
    }
}    /// <summary>
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
            if (instance is not IHotReloadable hotReloadable)
            {
                return; // Not a hot-reloadable system
            }

            if (!s_savedSystemStateByAssemblyPath.TryGetValue(assemblyPath, out var stateDict))
            {
                Console.WriteLine($"[StatePreserver] No saved state found for assembly: {assemblyPath}");
                return;
            }

            string typeName = instance.GetType().FullName ?? "Unknown";
            
            if (!stateDict.TryGetValue(typeName, out var savedState))
            {
                Console.WriteLine($"[StatePreserver] No saved state found for type: {typeName}");
                return;
            }

            if (savedState == null)
            {
                Console.WriteLine($"[StatePreserver] Saved state was null for {typeName}");
                return;
            }

            try
            {
                hotReloadable.OnAfterReload(savedState);
                Console.WriteLine($"[StatePreserver] Restored state for {typeName} ({savedState.Length} bytes)");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[StatePreserver] Error restoring state for {typeName}: {ex.Message}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[StatePreserver] Error in RestoreSystemState: {ex.Message}");
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
            s_savedSystemStateByAssemblyPath.Remove(assemblyPath);
        }
        else
        {
            s_savedSystemStateByAssemblyPath.Clear();
        }
    }
}
