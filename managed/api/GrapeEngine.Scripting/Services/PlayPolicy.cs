namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Audio playback policy for single-instance sounds.
/// </summary>
public enum PlayPolicy
{
    NewInstance = 0,
    SingleInstanceRestart = 1,
    SingleInstanceResume = 2,
    SingleInstanceIgnore = 3
}
