using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace GrapeEngine.Scripting.Internal.Hosting;

internal static partial class AssemblyManager
{

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
                    Thread.Yield();
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
                    Thread.Yield();
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
                catch (Exception ex) when (IsRecoverableHostingException(ex))
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
                        catch (Exception ex) when (IsRecoverableHostingException(ex)) { Logging.LogInternal($"[AssemblyManager] Could not delete companion file {Path.GetFileName(file)}: {ex.Message}", LogLevel.Debug); }
                    }
                }
                catch (Exception ex) when (IsRecoverableHostingException(ex)) { Logging.LogInternal($"[AssemblyManager] Failed to enumerate companion files with pattern {companionPattern}: {ex.Message}", LogLevel.Debug); }
            }

            if (deletedCount > 0)
            {
                Logging.LogInternal($"[AssemblyManager] Cleaned up {deletedCount} stale file(s) for {Path.GetFileName(originalPath)}", LogLevel.Info);
            }
        }
        catch (Exception ex) when (IsRecoverableHostingException(ex))
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
                catch (Exception dirEx) when (IsRecoverableHostingException(dirEx))
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
                catch (Exception pdbEx) when (IsRecoverableHostingException(pdbEx))
                {
                    Logging.LogInternal($"[AssemblyManager] Warning: Failed to write PDB: {pdbEx.Message}", LogLevel.Warning);
                }
            }

            // Return the versioned path - caller will load from this
            return versionedPath;
        }
        catch (Exception ex) when (IsRecoverableHostingException(ex))
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
                            try { File.Delete(pdbPath); } catch (Exception ex) when (IsRecoverableHostingException(ex)) { Logging.LogInternal($"[AssemblyManager] Could not delete companion PDB {Path.GetFileName(pdbPath)}: {ex.Message}", LogLevel.Debug); }
                        }
                    }
                    catch (Exception delEx) when (IsRecoverableHostingException(delEx))
                    {
                        Logging.LogInternal($"[AssemblyManager] Could not delete {Path.GetFileName(path)}: {delEx.Message}", LogLevel.Debug);
                    }
                }
            }
        }
        catch (Exception ex) when (IsRecoverableHostingException(ex))
        {
            Logging.LogInternal($"[AssemblyManager] Error in CleanupOldVersionedAssemblies: {ex.Message}", LogLevel.Warning);
        }
    }
}
