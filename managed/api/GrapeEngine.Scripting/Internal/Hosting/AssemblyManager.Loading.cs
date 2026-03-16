using System.IO;
using System.Reflection;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static partial class AssemblyManager
{

    private static string? FindLatestVersionedAssemblyPath(string originalAssemblyPath)
    {
        var dir = Path.GetDirectoryName(originalAssemblyPath) ?? "";
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
            return null;

        var filename = Path.GetFileNameWithoutExtension(originalAssemblyPath);
        var ext = Path.GetExtension(originalAssemblyPath);
        var pattern = $"{filename}_hotreload_*{ext}";

        // Parse numeric suffixes from "<name>_hotreload_<n>.dll" and select the highest n.
        // Non-matching files are ignored so corrupted or manual files do not break reload.
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
            bool wasLoaded;
            lock (s_sync)
            {
                wasLoaded = s_loadedAssemblies.ContainsKey(trackingKey);
            }

            if (wasLoaded)
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
            lock (s_sync)
            {
                s_loadedAssemblies[trackingKey] = (assembly, loadContext);
            }

            Logging.LogInternal($"[AssemblyManager] Loaded: {assembly.FullName}", LogLevel.Info);
            return assembly;
        }
        catch (Exception ex) when (IsRecoverableHostingException(ex))
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

            (Assembly Assembly, ScriptLoadContext? Context) entry;
            lock (s_sync)
            {
                if (!s_loadedAssemblies.TryGetValue(trackingKey, out entry))
                {
                    Logging.LogInternal($"[AssemblyManager] Assembly not loaded: {assemblyPath}", LogLevel.Warning);
                    return false;
                }

                // Remove only the target entry. Clearing all entries breaks multi-assembly tracking.
                s_loadedAssemblies.Remove(trackingKey);
            }

            Assembly? assembly = entry.Assembly;
            ScriptLoadContext? loadContext = entry.Context;
            string unloadedAssemblyName = assembly?.FullName ?? trackingKey;

            Logging.LogInternal($"[AssemblyManager] Unloading assembly: {trackingKey}", LogLevel.Info);

            Logging.LogInternal($"[AssemblyManager] Removed tracking entry: {trackingKey}", LogLevel.Debug);

            WeakReference? weakRef = null;
            if (loadContext != null)
            {
                weakRef = new WeakReference(loadContext);
                
                // Unload the load context to release the AssemblyDependencyResolver
                try
                {
                    loadContext.Unload();
                }
                catch (Exception ex) when (IsRecoverableHostingException(ex))
                {
                    Logging.LogInternal($"[AssemblyManager] Error during Unload/Dispose: {ex.Message}", LogLevel.Debug);
                }
            }

            // Drop strong references before forcing GC.
            // Keeping assembly/loadContext locals alive here can prevent collectible ALC unload.
            assembly = null;
            loadContext = null;

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

                Thread.Yield();
            }

            if (weakRef != null && weakRef.IsAlive)
            {
                Logging.LogInternal($"[AssemblyManager] ERROR: Load context still alive after all unload attempts for {trackingKey}", LogLevel.Error);
            }
            else
            {
                Logging.LogInternal($"[AssemblyManager] Confirmed: Assembly AssemblyLoadContext collected and unloaded: {trackingKey}", LogLevel.Info);
            }

            Logging.LogInternal($"[AssemblyManager] Unloaded: {unloadedAssemblyName}", LogLevel.Info);
            return true;
        }
        catch (Exception ex) when (IsRecoverableHostingException(ex))
        {
            Logging.LogInternal($"[AssemblyManager] Error unloading assembly: {ex.Message}", LogLevel.Error);
            return false;
        }
    }
}
