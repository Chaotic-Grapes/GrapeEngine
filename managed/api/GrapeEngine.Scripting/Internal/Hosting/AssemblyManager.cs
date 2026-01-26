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
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;

namespace GrapeEngine.Scripting.Internal.Hosting;

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
    private static readonly Dictionary<string, (Assembly Assembly, ScriptLoadContext? Context)> s_loadedAssemblies = new(StringComparer.OrdinalIgnoreCase);

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

    private static string? FindLatestVersionedAssemblyPath(string originalAssemblyPath)
    {
        var dir = Path.GetDirectoryName(originalAssemblyPath) ?? "";
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
            return null;

        var filename = Path.GetFileNameWithoutExtension(originalAssemblyPath);
        var ext = Path.GetExtension(originalAssemblyPath);
        var pattern = $"{filename}_hotreload_*{ext}";

        int bestVersion = -1;
        string? bestPath = null;

        foreach (var file in Directory.EnumerateFiles(dir, pattern))
        {
            var name = Path.GetFileNameWithoutExtension(file);
            int idx = name.LastIndexOf("_hotreload_", StringComparison.OrdinalIgnoreCase);
            if (idx <= 0)
                continue;

            var suffix = name.Substring(idx + "_hotreload_".Length);
            if (int.TryParse(suffix, out int version) && version > bestVersion)
            {
                bestVersion = version;
                bestPath = file;
            }
        }

        return bestPath;
    }

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
            bool isVersionedPath = assemblyPath.Contains("_hotreload_", StringComparison.OrdinalIgnoreCase);
            string trackingKey = NormalizeAssemblyKey(assemblyPath);

            string pathToLoad;
            if (isVersionedPath)
            {
                pathToLoad = assemblyPath;
            }
            else
            {
                // If we're asked to load the original path, prefer the most recent versioned build.
                var latest = FindLatestVersionedAssemblyPath(trackingKey);
                if (!string.IsNullOrEmpty(latest))
                {
                    pathToLoad = latest;
                    Logging.LogInternal($"[AssemblyManager] Found versioned assembly, loading: {pathToLoad}", LogLevel.Info);
                }
                else
                {
                    pathToLoad = trackingKey;
                    Logging.LogInternal($"[AssemblyManager] No versioned assembly found, loading original: {pathToLoad}", LogLevel.Info);
                }
            }

            // If this assembly is already loaded under the tracking key, unload it first.
            // Overwriting the dictionary entry without unloading would leak the old context.
            if (s_loadedAssemblies.ContainsKey(trackingKey))
            {
                Logging.LogInternal($"[AssemblyManager] Assembly already loaded, unloading previous instance: {trackingKey}", LogLevel.Info);
                UnloadAssembly(trackingKey);
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

            // Track by the (original) path so we can reload it consistently
            s_loadedAssemblies[trackingKey] = (assembly, loadContext);

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
            string trackingKey = NormalizeAssemblyKey(assemblyPath);

            if (!s_loadedAssemblies.TryGetValue(trackingKey, out var entry))
            {
                Logging.LogInternal($"[AssemblyManager] Assembly not loaded: {assemblyPath}", LogLevel.Warning);
                return false;
            }

            var (assembly, loadContext) = entry;

            Logging.LogInternal($"[AssemblyManager] Unloading assembly: {trackingKey}", LogLevel.Info);

            // Remove strong references COMPLETELY from the dictionary.
            // Dictionary.Remove() doesn't deallocate the Entry[] backing array - it just marks as deleted.
            // We need to clear the ENTIRE dictionary to force deallocation of the Entry[] array,
            // which was holding a strong reference to the Assembly and LoadContext.
            // This is CRITICAL for the AssemblyLoadContext to be garbage collected.
            int countBefore = s_loadedAssemblies.Count;
            s_loadedAssemblies.Clear();
            Logging.LogInternal($"[AssemblyManager] Cleared {countBefore} entries from loaded assemblies dictionary", LogLevel.Debug);

            WeakReference? weakRef = null;
            if (loadContext != null)
            {
                weakRef = new WeakReference(loadContext);
                
                // Unload the load context to release the AssemblyDependencyResolver
                try
                {
                    loadContext.Unload();
                    // Give the unload request a moment to propagate
                    System.Threading.Thread.Sleep(100);
                }
                catch (Exception ex)
                {
                    Logging.LogInternal($"[AssemblyManager] Error during Unload/Dispose: {ex.Message}", LogLevel.Debug);
                }
            }

            // Force GC to complete finalization and allow the ALC to unload.
            // Increased iterations and sleep time to handle cached references from World wrappers
            // and other assembly types. CoreCLR finalizers are asynchronous.
            for (int i = 0; i < 10; i++)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
                GC.Collect();

                if (weakRef == null || !weakRef.IsAlive)
                {
                    Logging.LogInternal($"[AssemblyManager] Assembly unloaded successfully on iteration {i}: {trackingKey}", LogLevel.Info);
                    break;
                }

                // Increased from 10ms to 50ms per iteration for more time for async finalizers
                // Total max time: 500ms (10 iterations × 50ms)
                System.Threading.Thread.Sleep(50);
            }

            if (weakRef != null && weakRef.IsAlive)
            {
                Logging.LogInternal($"[AssemblyManager] ERROR: Load context still alive after all unload attempts for {trackingKey}", LogLevel.Error);
            }
            else
            {
                Logging.LogInternal($"[AssemblyManager] Confirmed: Assembly AssemblyLoadContext collected and unloaded: {trackingKey}", LogLevel.Info);
            }

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
        string trackingKey = NormalizeAssemblyKey(assemblyPath);
        return s_loadedAssemblies.TryGetValue(trackingKey, out var entry) ? entry.Assembly : null;
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
        string trackingKey = NormalizeAssemblyKey(assemblyPath);
        return s_loadedAssemblies.ContainsKey(trackingKey);
    }

    /// <summary>
    /// Verify that a file is unlocked and can be written to.
    /// Retries with progressive backoff if file is locked.
    /// </summary>
    /// <param name="path">File path to check</param>
    /// <param name="maxRetries">Maximum number of retry attempts</param>
    /// <returns>true if file is unlocked or doesn't exist, false if still locked after retries</returns>
    public static bool VerifyFileUnlocked(string path, int maxRetries = 10)
    {
        // If file doesn't exist, it's already "unlocked"
        if (!File.Exists(path))
        {
            return true;
        }

        for (var i = 0; i < maxRetries; i++)
        {
            try
            {
                // Try to delete the file - this is the ultimate test of whether it's unlocked
                File.Delete(path);
                Logging.LogInternal($"[AssemblyManager] File deleted (was unlocked) after {i} attempts: {path}", LogLevel.Debug);
                return true; // Success - file was unlocked and is now deleted
            }
            catch (FileNotFoundException)
            {
                return true; // File doesn't exist anymore, that's fine
            }
            catch (DirectoryNotFoundException)
            {
                return true; // Directory doesn't exist, safe to create
            }
            catch (IOException)
            {
                // File is locked, retry with backoff
                if (i < maxRetries - 1)
                {
                    GC.Collect();
                    GC.WaitForPendingFinalizers();
                    System.Threading.Thread.Sleep(50 + i * 50); // Progressive backoff: 50, 100, 150ms...
                }
                else
                {
                    Logging.LogInternal($"[AssemblyManager] File still locked (IOException) after {maxRetries} attempts: {path}", LogLevel.Warning);
                    return false;
                }
            }
            catch (UnauthorizedAccessException accessEx)
            {
                // File is locked by system (antivirus, indexing, etc.) or permissions issue
                if (i < maxRetries - 1)
                {
                    Logging.LogInternal($"[AssemblyManager] Access denied to file (attempt {i + 1}/{maxRetries}), retrying: {path}", LogLevel.Debug);
                    GC.Collect();
                    GC.WaitForPendingFinalizers();
                    System.Threading.Thread.Sleep(50 + i * 50); // Progressive backoff
                }
                else
                {
                    Logging.LogInternal($"[AssemblyManager] File still locked (UnauthorizedAccessException) after {maxRetries} attempts: {accessEx.Message}", LogLevel.Warning);
                    return false;
                }
            }
        }
        return false;
    }

    /// <summary>
    /// Cleanup stale versioned assembly files on startup or before loading.
    /// Removes old _hotreload_* files that accumulated from previous sessions.
    /// </summary>
    /// <param name="originalPath">Original assembly path (e.g., GameScripts.dll)</param>
    public static void CleanupStaleVersionedAssemblies(string originalPath)
    {
        try
        {
            var dir = Path.GetDirectoryName(originalPath) ?? "";
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
                return;

            var filename = Path.GetFileNameWithoutExtension(originalPath);
            var ext = Path.GetExtension(originalPath);

            // Find all versioned DLL files
            var pattern = $"{filename}_hotreload_*{ext}";
            var staleFiles = Directory.GetFiles(dir, pattern);

            var deletedCount = 0;
            foreach (var file in staleFiles)
            {
                try
                {
                    File.Delete(file);
                    deletedCount++;
                }
                catch (Exception ex)
                {
                    // File might still be locked if editor crashed during hot reload
                    Logging.LogInternal($"[AssemblyManager] Could not delete stale file {Path.GetFileName(file)}: {ex.Message}", LogLevel.Debug);
                }
            }

            // Also cleanup companion files (.pdb, .xml, .deps.json)
            var companionExtensions = new[] { ".pdb", ".xml", ".deps.json" };
            foreach (var companionExt in companionExtensions)
            {
                var companionPattern = $"{filename}_hotreload_*{companionExt}";
                try
                {
                    foreach (var file in Directory.GetFiles(dir, companionPattern))
                    {
                        try
                        {
                            File.Delete(file);
                            deletedCount++;
                        }
                        catch { }
                    }
                }
                catch { }
            }

            if (deletedCount > 0)
            {
                Logging.LogInternal($"[AssemblyManager] Cleaned up {deletedCount} stale file(s) for {Path.GetFileName(originalPath)}", LogLevel.Info);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[AssemblyManager] Error in CleanupStaleVersionedAssemblies: {ex.Message}", LogLevel.Warning);
        }
    }

    /// <summary>
    /// Write compiled assembly to a versioned filename.
    /// 
    /// Strategy: Always write to a new versioned file (GameScripts_hotreload_1.dll, etc.)
    /// and return that path. Never try to overwrite or rename the original file.
    /// Old versioned files are cleaned up after successful load.
    /// 
    /// This avoids all file-locking issues that occur when trying to delete/rename locked files.
    /// </summary>
    /// <param name="originalPath">Original assembly path (e.g., GameScripts.dll) - used only to derive directory and name</param>
    /// <param name="compiledBytes">The compiled assembly bytes</param>
    /// <param name="pdbBytes">Optional PDB debug symbols bytes</param>
    /// <returns>Path to the newly written versioned assembly</returns>
    public static string? LoadVersionedAssembly(string originalPath, byte[] compiledBytes, byte[]? pdbBytes = null)
    {
        try
        {
            var dir = Path.GetDirectoryName(originalPath) ?? "";
            var filename = Path.GetFileNameWithoutExtension(originalPath);
            var ext = Path.GetExtension(originalPath);

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

            // Find the next available version number
            int version = 1;
            string versionedPath;
            while (File.Exists(versionedPath = Path.Combine(dir, $"{filename}_hotreload_{version}{ext}")))
            {
                version++;
            }

            Logging.LogInternal($"[AssemblyManager] Writing versioned assembly: {versionedPath}", LogLevel.Info);

            // Write DLL
            File.WriteAllBytes(versionedPath, compiledBytes);
            Logging.LogInternal($"[AssemblyManager] Wrote assembly: {versionedPath}", LogLevel.Info);

            // Write PDB if provided
            if (pdbBytes != null && pdbBytes.Length > 0)
            {
                string pdbPath = Path.ChangeExtension(versionedPath, ".pdb");
                try
                {
                    File.WriteAllBytes(pdbPath, pdbBytes);
                    Logging.LogInternal($"[AssemblyManager] Wrote PDB: {pdbPath}", LogLevel.Debug);
                }
                catch (Exception pdbEx)
                {
                    Logging.LogInternal($"[AssemblyManager] Warning: Failed to write PDB: {pdbEx.Message}", LogLevel.Warning);
                }
            }

            // Return the versioned path - caller will load from this
            return versionedPath;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[AssemblyManager] Error in LoadVersionedAssembly: {ex.Message}", LogLevel.Error);
            return null;
        }
    }

    /// <summary>
    /// Clean up old versioned assemblies, keeping only recent ones.
    /// Call this after successfully loading a new versioned assembly.
    /// </summary>
    /// <param name="originalPath">Original assembly path (e.g., GameScripts.dll)</param>
    /// <param name="keepCount">Number of recent versions to keep</param>
    public static void CleanupOldVersionedAssemblies(string originalPath, int keepCount = 3)
    {
        try
        {
            var dir = Path.GetDirectoryName(originalPath) ?? "";
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
                return;

            var filename = Path.GetFileNameWithoutExtension(originalPath);
            var ext = Path.GetExtension(originalPath);

            // Find all versioned files
            var versionedFiles = new List<(int version, string path)>();
            for (int v = 1; v <= 1000; v++)
            {
                string versionedPath = Path.Combine(dir, $"{filename}_hotreload_{v}{ext}");
                if (File.Exists(versionedPath))
                {
                    versionedFiles.Add((v, versionedPath));
                }
                else if (v > 10 && versionedFiles.Count > 0)
                {
                    // If we haven't found a file in 10 attempts after finding some, stop searching
                    break;
                }
            }

            // Keep only the most recent keepCount versions
            if (versionedFiles.Count > keepCount)
            {
                // Sort by version (descending) and delete older ones
                var toDelete = versionedFiles.OrderByDescending(x => x.version).Skip(keepCount).ToList();

                foreach (var (version, path) in toDelete)
                {
                    try
                    {
                        File.Delete(path);
                        Logging.LogInternal($"[AssemblyManager] Deleted old versioned assembly: {Path.GetFileName(path)}", LogLevel.Debug);

                        // Also delete associated PDB
                        string pdbPath = Path.ChangeExtension(path, ".pdb");
                        if (File.Exists(pdbPath))
                        {
                            try { File.Delete(pdbPath); } catch { }
                        }
                    }
                    catch (Exception delEx)
                    {
                        Logging.LogInternal($"[AssemblyManager] Could not delete {Path.GetFileName(path)}: {delEx.Message}", LogLevel.Debug);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[AssemblyManager] Error in CleanupOldVersionedAssemblies: {ex.Message}", LogLevel.Warning);
        }
    }
}



