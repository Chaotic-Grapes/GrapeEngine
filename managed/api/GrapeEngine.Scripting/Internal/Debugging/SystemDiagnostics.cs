using GrapeEngine.Math;
using System.Diagnostics;

namespace GrapeEngine.Scripting.Internal.Debugging;


/// <summary>
/// System diagnostics collector for debugging and profiling.
/// </summary>
public class SystemDiagnostics
{
    public string SystemName { get; set; } = string.Empty;
    public Type? SystemType { get; set; }
    public object? Instance { get; set; }
    public bool IsLoaded { get; set; }
    public DateTime LoadedAt { get; set; }
    public long TotalUpdateCalls { get; set; }
    public double AverageUpdateTimeMs { get; set; }
    public double MaxUpdateTimeMs { get; set; }
    public string LastError { get; set; } = string.Empty;
    public DateTime? LastErrorTime { get; set; }
}
