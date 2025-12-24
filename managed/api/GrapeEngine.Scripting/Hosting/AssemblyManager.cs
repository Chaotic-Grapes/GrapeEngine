/* Start Header *****************************************************************/
/*!
\file   AssemblyManager.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Manages assembly loading/unloading lifecycle.
*/
/* End Header *******************************************************************/

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// AssemblyManager - Encapsulates assembly loading and unloading logic.
/// 
/// Responsibilities:
/// - Load assemblies with custom AssemblyLoadContext for hot reload support
/// - Unload assemblies and free resources
/// - Track loaded assemblies and their contexts
/// - Handle assembly path resolution
/// </summary>
internal static class AssemblyManager
{
    /// <summary>
    /// Loaded assemblies and their load contexts (for hot reload support).
    /// Key: Assembly file path
    /// Value: (Assembly instance, Custom LoadContext for hot reload)
    /// </summary>
    private static readonly Dictionary<string, (Assembly Assembly, ScriptLoadContext? Context)> s_loadedAssemblies = [];

    /// <summary>
    /// Load a C# assembly from the specified path.
    /// 
    /// Uses custom ScriptLoadContext to enable hot reload (assembly unloading).
    /// Each assembly gets its own isolated load context.
    /// </summary>
    /// <param name="assemblyPath">Full path to .dll file to load</param>
    /// <returns>Loaded Assembly instance, or null if load failed</returns>
    public static Assembly? LoadAssembly(string assemblyPath)
    {
        try
        {
            if (!File.Exists(assemblyPath))
            {
                Logging.LogInternal($"[AssemblyManager] File not found: {assemblyPath}", LogLevel.Warning);
                return null;
            }

            Logging.LogInternal($"[AssemblyManager] Loading assembly: {assemblyPath}", LogLevel.Info);

            // Create a new load context for hot reload support
            var loadContext = new ScriptLoadContext(assemblyPath);
            Assembly assembly = loadContext.LoadFromAssemblyPath(assemblyPath);

            s_loadedAssemblies[assemblyPath] = (assembly, loadContext);

            Logging.LogInternal($"[AssemblyManager] Loaded: {assembly.FullName}", LogLevel.Info);
            return assembly;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[AssemblyManager] Failed to load assembly: {ex.Message}", LogLevel.Error);
            return null;
        }
    }

    /// <summary>
    /// Unload a previously loaded assembly.
    /// 
    /// Unloads the custom AssemblyLoadContext, forcing garbage collection of
    /// all types and instances from that assembly. Allows the DLL file to be
    /// rewritten on disk.
    /// </summary>
    /// <param name="assemblyPath">Full path to .dll that was loaded</param>
    /// <returns>true if unload successful, false if assembly not found</returns>
    public static bool UnloadAssembly(string assemblyPath)
    {
        try
        {
            if (!s_loadedAssemblies.TryGetValue(assemblyPath, out var entry))
            {
                Logging.LogInternal($"[AssemblyManager] Assembly not loaded: {assemblyPath}", LogLevel.Warning);
                return false;
            }

            var (assembly, loadContext) = entry;

            Logging.LogInternal($"[AssemblyManager] Unloading assembly: {assemblyPath}", LogLevel.Info);

            if (loadContext != null)
            {
                loadContext.Unload();
                // Force GC to complete finalization
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }

            s_loadedAssemblies.Remove(assemblyPath);
            Logging.LogInternal($"[AssemblyManager] Unloaded: {assembly.FullName}", LogLevel.Info);
            return true;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[AssemblyManager] Error unloading assembly: {ex.Message}", LogLevel.Error);
            return false;
        }
    }

    /// <summary>
    /// Get a previously loaded assembly by path.
    /// </summary>
    public static Assembly? GetLoadedAssembly(string assemblyPath)
    {
        return s_loadedAssemblies.TryGetValue(assemblyPath, out var entry) ? entry.Assembly : null;
    }

    /// <summary>
    /// Get all currently loaded assemblies.
    /// </summary>
    public static IEnumerable<Assembly> GetAllLoadedAssemblies()
    {
        return s_loadedAssemblies.Values.Select(x => x.Assembly);
    }

    /// <summary>
    /// Check if an assembly is currently loaded.
    /// </summary>
    public static bool IsAssemblyLoaded(string assemblyPath)
    {
        return s_loadedAssemblies.ContainsKey(assemblyPath);
    }
}
