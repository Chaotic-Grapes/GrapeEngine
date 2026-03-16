using GrapeEngine.Math;
using System.Diagnostics;

namespace GrapeEngine.Scripting.Internal.Debugging;


/// <summary>
/// Configuration for debugging features.
/// </summary>
public static class DebugConfiguration
{
    /// <summary>
    /// Enable verbose logging of script system operations.
    /// </summary>
    public static bool VerboseLogging { get; set; } = false;

    /// <summary>
    /// Enable automatic diagnostics collection.
    /// </summary>
    public static bool EnableDiagnostics { get; set; } = true;

    /// <summary>
    /// Break into debugger on script system errors.
    /// </summary>
    public static bool BreakOnError { get; set; } = false;

    /// <summary>
    /// Enable stack trace capture on errors.
    /// </summary>
    public static bool CaptureStackTraces { get; set; } = true;

    /// <summary>
    /// Maximum stack trace depth to capture.
    /// </summary>
    public static int MaxStackTraceDepth { get; set; } = 20;
}
