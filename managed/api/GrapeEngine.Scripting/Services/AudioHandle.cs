using GrapeEngine.Math;

namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Audio playback handle returned when playing a sound.
/// </summary>
public struct AudioHandle(ulong id)
{
    public ulong Id = id;

    public void SetVolume(float volume) => Audio.SetInstanceVolume(this, volume);
    public void SetPitch(float pitch) => Audio.SetInstancePitch(this, pitch);
    public void SetPan(float pan) => Audio.SetInstancePan(this, pan);
    public void SetLowPassFilter(float gain) => Audio.SetLowPassFilter(this, gain);
    public void ClearLowPassFilter() => Audio.ClearLowPassFilter(this);
    public void SetPosition(Vector3 position, Vector3 velocity) => Audio.SetInstancePosition(this, position, velocity);
    public void Stop(StopMode mode = StopMode.Immediate) => Audio.Stop(this, mode);
}
