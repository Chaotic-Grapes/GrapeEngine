namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Log level for script logging. Ordered by severity for filtering.
/// </summary>
public enum LogLevel : byte
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
}
