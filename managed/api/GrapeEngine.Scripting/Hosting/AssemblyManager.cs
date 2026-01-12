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
    /// 
    /// If the original path doesn't exist but a newer versioned assembly does,
    /// loads the latest versioned assembly instead.
    /// </summary>
    /// <param name="assemblyPath">Full path to .dll file to load</param>
    /// <returns>Loaded Assembly instance, or null if load failed</returns>
    public static Assembly? LoadAssembly(string assemblyPath)
    {
        try
        {
            // If the original path doesn't exist, look for the latest versioned assembly
            string pathToLoad = assemblyPath;
            if (!File.Exists(pathToLoad))
            {
                string dir = Path.GetDirectoryName(assemblyPath) ?? "";
                string filename = Path.GetFileNameWithoutExtension(assemblyPath);
                string ext = Path.GetExtension(assemblyPath);

                // Look for versioned assemblies
                int latestVersion = 0;
                string? latestVersionedPath = null;

                for (int v = 1; v <= 100; v++)
                {
                    string versionedPath = Path.Combine(dir, $"{filename}_hotreload_{v}{ext}");
                    if (File.Exists(versionedPath))
                    {
                        latestVersion = v;
                        latestVersionedPath = versionedPath;
                    }
                    else if (latestVersion > 0)
                    {
                        // Found a gap, stop searching
                        break;
                    }
                }

                if (latestVersionedPath != null)
                {
                    pathToLoad = latestVersionedPath;
                    Logging.LogInternal($"[AssemblyManager] Original not found, using versioned: {pathToLoad}", LogLevel.Info);
                }
            }

            if (!File.Exists(pathToLoad))
            {
                Logging.LogInternal($"[AssemblyManager] File not found: {pathToLoad}", LogLevel.Warning);
                return null;
            }

            Logging.LogInternal($"[AssemblyManager] Loading assembly: {pathToLoad}", LogLevel.Info);

            // Create a new load context for hot reload support
            var loadContext = new ScriptLoadContext(pathToLoad);
            Assembly assembly = loadContext.LoadFromAssemblyPath(pathToLoad);

            // Track by the original path so we can reload it consistently
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

    /// <summary>
    /// Write compiled assembly bytes and clean up old versions.
    /// 
    /// Creates a temporary versioned file during compilation, then attempts to delete all
    /// old versions and rename the new version to the original filename.
    /// If the original file is still locked (even after unload), retries with backoff.
    /// </summary>
    /// <param name="originalPath">Original assembly path (e.g., GameScripts.dll)</param>
    /// <param name="compiledBytes">The compiled assembly bytes</param>
    /// <returns>Original path (since the new assembly is renamed to it)</returns>
    public static string? LoadVersionedAssembly(string originalPath, byte[] compiledBytes)
    {
        try
        {
            string dir = Path.GetDirectoryName(originalPath) ?? "";
            string filename = Path.GetFileNameWithoutExtension(originalPath);
            string ext = Path.GetExtension(originalPath);

            // Ensure the output directory exists
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                try
                {
                    Directory.CreateDirectory(dir);
                    Logging.LogInternal($"[AssemblyManager] Created output directory: {dir}", LogLevel.Info);
                }
                catch (Exception dirEx)
                {
                    Logging.LogInternal($"[AssemblyManager] Failed to create directory {dir}: {dirEx.Message}", LogLevel.Error);
                    return null;
                }
            }

            // Write to a temporary versioned file first (in case the original still exists/is locked)
            int version = 1;
            string tempVersionedPath;
            while (File.Exists(tempVersionedPath = Path.Combine(dir, $"{filename}_hotreload_{version}{ext}")))
            {
                version++;
            }

            Logging.LogInternal($"[AssemblyManager] LoadVersionedAssembly: dir='{dir}', filename='{filename}', ext='{ext}'", LogLevel.Debug);

            File.WriteAllBytes(tempVersionedPath, compiledBytes);
            Logging.LogInternal($"[AssemblyManager] Wrote temporary assembly: {tempVersionedPath}", LogLevel.Info);

            // Attempt to clean up old files - try multiple times with backoff since file locks can persist
            const int maxRetries = 3;
            const int retryDelayMs = 100;

            for (int attempt = 0; attempt < maxRetries; attempt++)
            {
                try
                {
                    // Force aggressive garbage collection to release any lingering references
                    GC.Collect();
                    GC.WaitForPendingFinalizers();
                    GC.Collect();

                    if (attempt > 0)
                    {
                        // Wait before retrying
                        System.Threading.Thread.Sleep(retryDelayMs * (int)System.Math.Pow(2, attempt - 1));
                    }

                    // Delete the original if it exists (old assembly should be unloaded by now)
                    if (File.Exists(originalPath))
                    {
                        try
                        {
                            File.Delete(originalPath);
                            Logging.LogInternal($"[AssemblyManager] Deleted old assembly: {originalPath}", LogLevel.Debug);
                        }
                        catch (Exception delEx)
                        {
                            // File is locked, will try again on next iteration or keep the versioned file
                            if (attempt < maxRetries - 1)
                            {
                                Logging.LogInternal($"[AssemblyManager] Could not delete old assembly (attempt {attempt + 1}/{maxRetries}): {delEx.Message}", LogLevel.Debug);
                                continue;
                            }
                            else
                            {
                                Logging.LogInternal($"[AssemblyManager] Could not delete old assembly after {maxRetries} attempts: {delEx.Message}", LogLevel.Warning);
                            }
                        }
                    }

                    // Delete ALL old versioned files
                    for (int v = 1; v < version; v++)
                    {
                        string oldVersionedPath = Path.Combine(dir, $"{filename}_hotreload_{v}{ext}");
                        if (File.Exists(oldVersionedPath))
                        {
                            try
                            {
                                File.Delete(oldVersionedPath);
                                Logging.LogInternal($"[AssemblyManager] Deleted old version: {oldVersionedPath}", LogLevel.Debug);
                            }
                            catch (Exception delEx)
                            {
                                Logging.LogInternal($"[AssemblyManager] Could not delete old version {oldVersionedPath}: {delEx.Message}", LogLevel.Warning);
                            }
                        }
                    }

                    // Try to rename the temporary versioned file back to the original name
                    try
                    {
                        File.Move(tempVersionedPath, originalPath, overwrite: true);
                        Logging.LogInternal($"[AssemblyManager] Renamed to original: {originalPath}", LogLevel.Info);
                        return originalPath;
                    }
                    catch (Exception renameEx)
                    {
                        if (attempt < maxRetries - 1)
                        {
                            Logging.LogInternal($"[AssemblyManager] Could not rename (attempt {attempt + 1}/{maxRetries}): {renameEx.Message}", LogLevel.Debug);
                            continue;
                        }
                        else
                        {
                            // Final attempt failed - keep the versioned file and return that path instead
                            Logging.LogInternal($"[AssemblyManager] Could not rename to original after {maxRetries} attempts. File will remain as: {tempVersionedPath}", LogLevel.Warning);
                            return tempVersionedPath;
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logging.LogInternal($"[AssemblyManager] Unexpected error in retry loop (attempt {attempt + 1}): {ex.Message}", LogLevel.Warning);
                }
            }

            // If we get here, return the versioned path as fallback
            return tempVersionedPath;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[AssemblyManager] Error in LoadVersionedAssembly: {ex.Message}", LogLevel.Error);
            return null;
        }
    }
}

