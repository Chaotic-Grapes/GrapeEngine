namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Audio mixer bus.
/// </summary>
public enum AudioBus : byte
{
    Master = 0,
    Music = 1,
    SFX = 2,
    UI = 3,
    Ambient = 4
}
