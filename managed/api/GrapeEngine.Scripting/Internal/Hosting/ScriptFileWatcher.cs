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

namespace GrapeEngine.Scripting.Internal.Hosting;

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

    // Ignore paths that commonly change during builds/compilation and can cause feedback loops.
    // NOTE: FileSystemWatcher doesn't support excludes, so we filter in handlers.
    private static readonly string[] _ignoredPathSegments =
    [
        "\\\\.git\\\\",
        "\\\\.vs\\\\",
        "\\\\.idea\\\\",
        "\\\\bin\\\\",
        "\\\\obj\\\\",
        "\\\\build\\\\",
        "\\\\x64\\\\",
        "\\\\Debug\\\\",
        "\\\\Release\\\\",
        "\\\\.vscode\\\\",
    ];

    private static bool IsCsFilePath(string fullPath)
        => fullPath.EndsWith(".cs", StringComparison.OrdinalIgnoreCase);

    private static bool ShouldIgnorePath(string fullPath)
    {
        if (string.IsNullOrWhiteSpace(fullPath))
            return true;

        var p = fullPath.Replace('/', '\\');
        p = p.ToLowerInvariant();

        for (int i = 0; i < _ignoredPathSegments.Length; i++)
        {
            if (p.Contains(_ignoredPathSegments[i].ToLowerInvariant()))
                return true;
        }

        // Ignore common generated C# sources even if they somehow appear outside ignored dirs.
        if (p.EndsWith(".g.cs", StringComparison.Ordinal) || p.EndsWith(".generated.cs", StringComparison.Ordinal))
            return true;

        return false;
    }

    /// <summary>
    /// Delegate for native C++ callback when C# files change.
    /// C++ decides what to do (compile, reload, etc.)
    /// Signature: void callback(const char* scriptDirectory)
    /// </summary>
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void NativeScriptChangedCallback(string directoryPath);

    /// <summary>
    /// Function pointer to native callback (set by C++ during initialization).
    /// </summary>
    private static IntPtr _nativeScriptChangedCallback = IntPtr.Zero;

    /// <summary>
    /// Start watching a directory for C# file changes.
    /// Called from C++ via interop.
    /// 
    /// C# only detects and notifies; C++ decides what to do.
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

            // Remember watched directory
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
        catch (Exception ex) when (ex is not OutOfMemoryException and not StackOverflowException)
        {
            Logging.LogInternal($"[ScriptFileWatcher] Error starting watcher: {ex.Message}", LogLevel.Error);
            return -1;
        }
    }

    /// <summary>
    /// Register the native callback for when C# files change.
    /// Called by C++ to set up the notification channel.
    /// 
    /// C++ will be notified when files change, then decides orchestration.
    /// </summary>
    [UnmanagedCallersOnly]
    public static void RegisterScriptChangedCallback(IntPtr callbackPtr)
    {
        _nativeScriptChangedCallback = callbackPtr;
        Logging.LogInternal("[ScriptFileWatcher] Script changed callback registered", LogLevel.Info);
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
        if (!IsCsFilePath(e.FullPath) || ShouldIgnorePath(e.FullPath))
            return;

        if (e.ChangeType == WatcherChangeTypes.Changed && !File.Exists(e.FullPath))
            return;

        lock (_lock)
        {
            // Add to changed files set
            _changedFiles.Add(e.FullPath);
            
            // Reset debounce timer
            _debounceTimer?.Stop();
            _debounceTimer?.Start();
        }

        // Per-file event logging is extremely noisy and can impact editor responsiveness.
        Logging.LogInternal($"[ScriptFileWatcher] Detected change: {e.ChangeType} - {e.Name}", LogLevel.Debug);
    }

    private static void OnFileRenamed(object sender, RenamedEventArgs e)
    {
        lock (_lock)
        {
            if (IsCsFilePath(e.OldFullPath) && !ShouldIgnorePath(e.OldFullPath))
            {
                _changedFiles.Add(e.OldFullPath);
            }

            if (IsCsFilePath(e.FullPath) && !ShouldIgnorePath(e.FullPath))
            {
                _changedFiles.Add(e.FullPath);
            }
            
            _debounceTimer?.Stop();
            _debounceTimer?.Start();
        }

        Logging.LogInternal($"[ScriptFileWatcher] Detected rename: {e.OldName} -> {e.Name}", LogLevel.Debug);
    }

    private static void OnDebounceTimerElapsed(object? sender, System.Timers.ElapsedEventArgs e)
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

        // Notify internal callback (if registered)
        foreach (var file in filesToProcess)
        {
            _onFileChanged?.Invoke(file);
        }

        // Notify C++ that scripts changed.
        // C++ will decide what to do: compile, reload, etc.
        // This is just a notification, not orchestration.
        try
        {
            if (!string.IsNullOrEmpty(_watchedDirectory) && _nativeScriptChangedCallback != IntPtr.Zero)
            {
                Logging.LogInternal($"[ScriptFileWatcher] Notifying C++ that scripts changed", LogLevel.Info);

                var callback = Marshal.GetDelegateForFunctionPointer<NativeScriptChangedCallback>(_nativeScriptChangedCallback);
                callback?.Invoke(_watchedDirectory);
            }
            else
            {
                Logging.LogInternal("[ScriptFileWatcher] No C++ callback registered", LogLevel.Warning);
            }
        }
        catch (Exception ex) when (ex is not OutOfMemoryException and not StackOverflowException)
        {
            Logging.LogInternal($"[ScriptFileWatcher] Error notifying C++ of script changes: {ex.Message}", LogLevel.Error);
        }
    }
}



