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
using System.Text;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// File watcher for hot reload support.
/// Monitors C# script files and triggers reload on changes.
/// </summary>
public static class ScriptFileWatcher
{
    private static FileSystemWatcher? _watcher;
    private static System.Timers.Timer? _debounceTimer;
    private static readonly HashSet<string> _changedFiles = [];
    private static readonly Lock _lock = new();
    private static Action<string>? _onFileChanged;
    private static string? _watchedDirectory;
    private static string? _outputAssemblyPath;  // Output path for compiled scripts

    // Native compile status callback (function pointer set by native ScriptManager)
    // Signature: void callback(int status, int progress, sbyte* messageUtf8)
    private static unsafe delegate* unmanaged[Cdecl]<int, int, sbyte*, void> _compileCallback = null;

    /// <summary>
    /// Start watching a directory for C# file changes.
    /// Called from C++ via interop.
    /// directoryPathPtr: Source scripts directory to watch
    /// userData: Reserved for future use (normally would be output path as string)
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe int StartWatching(char* directoryPathPtr, void* userData)
    {
        try
        {
            var directoryPath = Marshal.PtrToStringUTF8((IntPtr)directoryPathPtr) ?? "";
            
            if (!Directory.Exists(directoryPath))
            {
                Logging.LogInternal($"[ScriptFileWatcher] Directory does not exist: {directoryPath}", LogLevel.Warning);
                return -1;
            }

            Logging.LogInternal($"[ScriptFileWatcher] Starting file watcher for: {directoryPath}", LogLevel.Info);

            // Remember watched directory for compile+reload
            _watchedDirectory = directoryPath;

            // Stop existing watcher if any
            StopWatchingImpl();

            // Create file system watcher
            _watcher = new FileSystemWatcher(directoryPath)
            {
                Filter = "*.cs",
                NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName | NotifyFilters.CreationTime,
                IncludeSubdirectories = true,
                EnableRaisingEvents = true
            };

            // Setup event handlers
            _watcher.Changed += OnFileChanged;
            _watcher.Created += OnFileChanged;
            _watcher.Deleted += OnFileChanged;
            _watcher.Renamed += OnFileRenamed;

            // Setup debounce timer (avoid multiple triggers for same file)
            _debounceTimer = new System.Timers.Timer(500); // 500ms debounce
            _debounceTimer.Elapsed += OnDebounceTimerElapsed;
            _debounceTimer.AutoReset = false;

            Logging.LogInternal($"[ScriptFileWatcher] File watcher started successfully", LogLevel.Info);
            return 0;
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptFileWatcher] Error starting watcher: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Set the native compile-status callback function pointer.
    /// Called by native code (ScriptManager) to register a callback that
    /// receives compile start/progress/finish notifications.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void SetCompileCallback(nint callbackPtr)
    {
        _compileCallback = (delegate* unmanaged[Cdecl]<int, int, sbyte*, void>)callbackPtr;
    }

    /// <summary>
    /// Set the output assembly path for hot reload compilations.
    /// Called from C++ to standardize where compiled scripts are written.
    /// </summary>
    [UnmanagedCallersOnly]
    public static unsafe void SetOutputAssemblyPath(char* outputPathPtr)
    {
        if (outputPathPtr != null)
        {
            _outputAssemblyPath = Marshal.PtrToStringUTF8((IntPtr)outputPathPtr);
            Logging.LogInternal($"[ScriptFileWatcher] Output assembly path set to: {_outputAssemblyPath}", LogLevel.Info);
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
        if (_watcher != null)
        {
            _watcher.EnableRaisingEvents = false;
            _watcher.Dispose();
            _watcher = null;
            Logging.LogInternal($"[ScriptFileWatcher] File watcher stopped", LogLevel.Info);
        }

        if (_debounceTimer != null)
        {
            _debounceTimer.Stop();
            _debounceTimer.Dispose();
            _debounceTimer = null;
        }
    }

    /// <summary>
    /// Register a callback for when files change (internal C# use).
    /// </summary>
    internal static void RegisterCallback(Action<string> callback)
    {
        _onFileChanged = callback;
    }

    private static void OnFileChanged(object sender, FileSystemEventArgs e)
    {
        lock (_lock)
        {
            // Add to changed files set
            _changedFiles.Add(e.FullPath);
            
            // Reset debounce timer
            _debounceTimer?.Stop();
            _debounceTimer?.Start();
            
            Logging.LogInternal($"[ScriptFileWatcher] Detected change: {e.ChangeType} - {e.Name}", LogLevel.Info);
        }
    }

    private static void OnFileRenamed(object sender, RenamedEventArgs e)
    {
        lock (_lock)
        {
            _changedFiles.Add(e.FullPath);
            
            _debounceTimer?.Stop();
            _debounceTimer?.Start();
            
            Logging.LogInternal($"[ScriptFileWatcher] Detected rename: {e.OldName} -> {e.Name}", LogLevel.Info);
        }
    }

    private static unsafe void OnDebounceTimerElapsed(object? sender, System.Timers.ElapsedEventArgs e)
    {
        HashSet<string> filesToProcess;
        lock (_lock)
        {
            filesToProcess = [.. _changedFiles];
            _changedFiles.Clear();
        }

        if (filesToProcess.Count == 0)
        {
            return;
        }

        Logging.LogInternal($"[ScriptFileWatcher] Processing {filesToProcess.Count} changed file(s)", LogLevel.Info);

        // Notify callback (if registered)
        foreach (var file in filesToProcess)
        {
            _onFileChanged?.Invoke(file);
        }

        // Trigger recompilation and reload in background; report status via callback
        try
        {
            if (!string.IsNullOrEmpty(_watchedDirectory))
            {
                // Use the output assembly path if set, otherwise default to temp location
                // For consistency, this should always be set to the temp path by C++
                string outPath = !string.IsNullOrEmpty(_outputAssemblyPath) 
                    ? _outputAssemblyPath
                    : Path.Combine(_watchedDirectory, "GameScripts.dll");
                    
                Logging.LogInternal($"[ScriptFileWatcher] Hot reload triggered - compiling and reloading: {_watchedDirectory} -> {outPath}", LogLevel.Info);

                // Notify native that compilation started (status 1, progress indeterminate (-1))
                if (_compileCallback != null)
                {
                    unsafe
                    {
                        IntPtr p = ToUtf8Ptr("Compiling...");
                        try 
                        { 
                            _compileCallback(1, -1, (sbyte*)p.ToPointer()); 
                        }
                        finally
                        { 
                            if (p != IntPtr.Zero)
                                Marshal.FreeHGlobal(p); 
                        }
                    }
                }

                // Run compile+reload asynchronously so file-watcher thread isn't blocked
                Task.Run(() => 
                {
                    var r = ScriptHost.TriggerCompileAndReloadManaged(_watchedDirectory, outPath);

                    // Collect Roslyn diagnostics (if any) so we can forward full details to native callback
                    var diags = RoslynCompiler.GetLastDiagnostics() ?? string.Empty;

                    // If diagnostics are empty, fallback to simple messages
                    string msg;
                    if (!string.IsNullOrWhiteSpace(diags)) 
                    {
                        msg = diags;
                    }
                    else 
                    {
                        msg = r == 0 ? "OK" : "Compilation failed";
                    }

                    // Notify native of completion: status 3 = success, 4 = failure
                    if (_compileCallback != null)
                    {
                        unsafe
                        {
                            IntPtr pmsg = ToUtf8Ptr(msg);
                            try
                            {
                                _compileCallback(r == 0 ? 3 : 4, r == 0 
                                    ? 100 
                                    : 0, (sbyte*)pmsg.ToPointer()); 
                            }
                            finally
                            { 
                                if (pmsg != IntPtr.Zero)
                                    Marshal.FreeHGlobal(pmsg); 
                            }
                        }
                    }
                });
            }
            else
            {
                Logging.LogInternal("[ScriptFileWatcher] No watched directory registered for hot reload", LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[ScriptFileWatcher] Error during hot reload: {ex.Message}", LogLevel.Error);
        }
    }

    private static IntPtr ToUtf8Ptr(string s)
    {
        var bytes = Encoding.UTF8.GetBytes(s + '\0');
        IntPtr p = Marshal.AllocHGlobal(bytes.Length);
        Marshal.Copy(bytes, 0, p, bytes.Length);

        return p;
    }
}
