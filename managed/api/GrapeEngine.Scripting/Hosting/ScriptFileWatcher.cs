/* Start Header *****************************************************************/
/*!
\file   ScriptFileWatcher.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
File watcher for C# script hot reload support.
Monitors script files and triggers recompilation/reload on changes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// File watcher for hot reload support.
/// Monitors C# script files and triggers reload on changes.
/// </summary>
public static class ScriptFileWatcher
{
    private static FileSystemWatcher? s_watcher;
    private static System.Timers.Timer? s_debounceTimer;
    private static readonly HashSet<string> s_changedFiles = [];
    private static readonly Lock _lock = new();
    private static Action<string>? s_onFileChanged;

    /// <summary>
    /// Start watching a directory for C# file changes.
    /// Called from C++ via interop.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int StartWatching(char* directoryPathPtr, void* callbackPtr)
    {
        try
        {
            string directoryPath = Marshal.PtrToStringUTF8((IntPtr)directoryPathPtr) ?? "";
            
            if (!Directory.Exists(directoryPath))
            {
                Console.WriteLine($"[ScriptFileWatcher] Directory does not exist: {directoryPath}");
                return -1;
            }

            Console.WriteLine($"[ScriptFileWatcher] Starting file watcher for: {directoryPath}");

            // Stop existing watcher if any
            StopWatchingImpl();

            // Create file system watcher
            s_watcher = new FileSystemWatcher(directoryPath)
            {
                Filter = "*.cs",
                NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName | NotifyFilters.CreationTime,
                IncludeSubdirectories = true,
                EnableRaisingEvents = true
            };

            // Setup event handlers
            s_watcher.Changed += OnFileChanged;
            s_watcher.Created += OnFileChanged;
            s_watcher.Deleted += OnFileChanged;
            s_watcher.Renamed += OnFileRenamed;

            // Setup debounce timer (avoid multiple triggers for same file)
            s_debounceTimer = new System.Timers.Timer(500); // 500ms debounce
            s_debounceTimer.Elapsed += OnDebounceTimerElapsed;
            s_debounceTimer.AutoReset = false;

            Console.WriteLine($"[ScriptFileWatcher] File watcher started successfully");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ScriptFileWatcher] Error starting watcher: {ex.Message}");
            return -1;
        }
    }

    /// <summary>
    /// Stop watching for file changes.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void StopWatching()
        => StopWatchingImpl();

    private static void StopWatchingImpl()
    {
        if (s_watcher != null)
        {
            s_watcher.EnableRaisingEvents = false;
            s_watcher.Dispose();
            s_watcher = null;
            Console.WriteLine($"[ScriptFileWatcher] File watcher stopped");
        }

        if (s_debounceTimer != null)
        {
            s_debounceTimer.Stop();
            s_debounceTimer.Dispose();
            s_debounceTimer = null;
        }
    }

    /// <summary>
    /// Register a callback for when files change (internal C# use).
    /// </summary>
    internal static void RegisterCallback(Action<string> callback)
    {
        s_onFileChanged = callback;
    }

    private static void OnFileChanged(object sender, FileSystemEventArgs e)
    {
        lock (_lock)
        {
            // Add to changed files set
            s_changedFiles.Add(e.FullPath);
            
            // Reset debounce timer
            s_debounceTimer?.Stop();
            s_debounceTimer?.Start();
            
            Console.WriteLine($"[ScriptFileWatcher] Detected change: {e.ChangeType} - {e.Name}");
        }
    }

    private static void OnFileRenamed(object sender, RenamedEventArgs e)
    {
        lock (_lock)
        {
            s_changedFiles.Add(e.FullPath);
            
            s_debounceTimer?.Stop();
            s_debounceTimer?.Start();
            
            Console.WriteLine($"[ScriptFileWatcher] Detected rename: {e.OldName} -> {e.Name}");
        }
    }

    private static void OnDebounceTimerElapsed(object? sender, System.Timers.ElapsedEventArgs e)
    {
        HashSet<string> filesToProcess;
        lock (_lock)
        {
            filesToProcess = [.. s_changedFiles];
            s_changedFiles.Clear();
        }

        if (filesToProcess.Count == 0)
        {
            return;
        }

        Console.WriteLine($"[ScriptFileWatcher] Processing {filesToProcess.Count} changed file(s)");

        // Notify callback (if registered)
        foreach (var file in filesToProcess)
        {
            s_onFileChanged?.Invoke(file);
        }

        // TODO: Trigger recompilation and reload
        // For now, just log that changes were detected
        Console.WriteLine($"[ScriptFileWatcher] Hot reload triggered for changed files");
    }
}
