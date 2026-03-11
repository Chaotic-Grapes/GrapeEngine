/* Start Header *****************************************************************/
/*!
\file   Audio.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
Provides access to audio playback and control functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Math;
using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Core;

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

/// <summary>
/// Audio stop mode.
/// </summary>
public enum StopMode
{
    Immediate = 0,
    AllowFadeOut = 1
}

/// <summary>
/// Provides access to audio playback and control functionality.
/// </summary>
public static class Audio
{
    // ============================================================================
    // Sound Loading
    // ============================================================================

    /// <summary>
    /// Load an audio cue from a file.
    /// </summary>
    /// <param name="cueId">Unique identifier for the cue.</param>
    /// <param name="filePath">Path to the audio file.</param>
    /// <param name="is3D">Whether this is a 3D positional sound.</param>
    /// <param name="isLooping">Whether the sound should loop.</param>
    /// <param name="isStreaming">Whether to stream from disk (recommended for music).</param>
    /// <returns>True if loaded successfully.</returns>
    public static bool LoadCue(string cueId, string filePath, bool is3D = false, bool isLooping = false, bool isStreaming = false)
        => AudioAPI.LoadCue(cueId, filePath, is3D, isLooping, isStreaming);

    /// <summary>
    /// Unload a previously loaded audio cue.
    /// </summary>
    public static void UnloadCue(string cueId) => AudioAPI.UnloadCue(cueId);

    /// <summary>
    /// Check if a cue is currently loaded.
    /// </summary>
    public static bool HasCue(string cueId) => AudioAPI.HasCue(cueId);

    // ============================================================================
    // Sound Playback
    // ============================================================================

    /// <summary>
    /// Play a sound cue and return a handle to control it.
    /// </summary>
    /// <param name="cueId">The cue ID to play.</param>
    /// <param name="volume">Volume (0.0 to 1.0).</param>
    /// <param name="pitch">Pitch multiplier (default 1.0).</param>
    /// <param name="paused">Start paused.</param>
    /// <returns>Audio handle for controlling playback.</returns>
    public static AudioHandle Play(string cueId, float volume = 1.0f, float pitch = 1.0f, bool paused = false)
    {
        return new(AudioAPI.Play(cueId, volume, pitch, paused));
    }

    /// <summary>
    /// Play a sound cue with single-instance policy (prevents overlapping plays).
    /// </summary>
    /// <param name="cueId">The cue ID to play.</param>
    /// <param name="policy">Policy for handling multiple play requests.</param>
    /// <param name="volume">Volume (0.0 to 1.0).</param>
    /// <param name="pitch">Pitch multiplier (default 1.0).</param>
    /// <returns>Audio handle for controlling playback.</returns>
    public static AudioHandle PlaySingle(string cueId, PlayPolicy policy = PlayPolicy.SingleInstanceIgnore, float volume = 1.0f, float pitch = 1.0f)
    {
        return new(AudioAPI.PlaySingle(cueId, volume, pitch, (int)policy));
    }

    /// <summary>
    /// Stop a playing sound instance.
    /// </summary>
    public static void Stop(AudioHandle handle, StopMode mode = StopMode.Immediate)
    {
        AudioAPI.Stop(handle.Id, (int)mode);
    }

    /// <summary>
    /// Stop all instances of a cue.
    /// </summary>
    public static void StopCue(string cueId, StopMode mode = StopMode.Immediate)
    {
        AudioAPI.StopCue(cueId, (int)mode);
    }

    /// <summary>
    /// Check if a cue is currently playing.
    /// </summary>
    public static bool IsCuePlaying(string cueId)
    {
        return AudioAPI.IsCuePlaying(cueId);
    }

    // ============================================================================
    // Instance Control
    // ============================================================================

    /// <summary>
    /// Set the volume of a playing sound instance.
    /// </summary>
    public static void SetInstanceVolume(AudioHandle handle, float volume)
    {
        AudioAPI.SetInstanceVolume(handle.Id, volume);
    }

    /// <summary>
    /// Set the pitch of a playing sound instance.
    /// </summary>
    public static void SetInstancePitch(AudioHandle handle, float pitch)
    {
        AudioAPI.SetInstancePitch(handle.Id, pitch);
    }

    /// <summary>
    /// Set the stereo pan of a playing sound instance (2D only).
    /// </summary>
    public static void SetInstancePan(AudioHandle handle, float pan)
    {
        AudioAPI.SetInstancePan(handle.Id, pan);
    }

    /// <summary>
    /// Set low-pass filter gain for a playing sound instance.
    /// 1.0 = no filtering, 0.0 = strongest low-pass.
    /// </summary>
    public static void SetLowPassFilter(AudioHandle handle, float gain)
    {
        float clamped = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
        AudioAPI.SetInstanceLowPassGain(handle.Id, clamped);
    }

    /// <summary>
    /// Clear low-pass filtering for a playing sound instance.
    /// </summary>
    public static void ClearLowPassFilter(AudioHandle handle)
    {
        AudioAPI.SetInstanceLowPassGain(handle.Id, 1.0f);
    }

    /// <summary>
    /// Set the 3D position and velocity of a playing sound instance.
    /// </summary>
    public static void SetInstancePosition(AudioHandle handle, Vector3 position, Vector3 velocity)
    {
        AudioAPI.SetInstancePosition(handle.Id, position.X, position.Y, position.Z, velocity.X, velocity.Y, velocity.Z);
    }

    // ============================================================================
    // Master Controls
    // ============================================================================

    /// <summary>
    /// Get or set the master volume (affects all sounds).
    /// </summary>
    public static float MasterVolume
    {
        get => AudioAPI.GetMasterVolume();
        set => AudioAPI.SetMasterVolume(value);
    }

    /// <summary>
    /// Set the volume for a specific mixer bus.
    /// </summary>
    public static void SetBusVolume(AudioBus bus, float volume)
    {
        AudioAPI.SetBusVolume((int)bus, volume);
    }

    /// <summary>
    /// Get the volume for a specific mixer bus.
    /// </summary>
    public static float GetBusVolume(AudioBus bus)
    {
        return AudioAPI.GetBusVolume((int)bus);
    }

    /// <summary>
    /// Fade a mixer bus to a target volume over time.
    /// </summary>
    public static void FadeBusVolume(AudioBus bus, float targetVolume, float duration)
    {
        AudioAPI.FadeBusVolume((int)bus, targetVolume, duration);
    }

    /// <summary>
    /// Set low-pass filtering for a specific mixer bus.
    /// 1.0 = no filtering, 0.0 = strongest filtering.
    /// </summary>
    public static void SetBusLowPassFilter(AudioBus bus, float gain)
    {
        float clamped = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
        AudioAPI.SetBusLowPassGain((int)bus, clamped);
    }

    /// <summary>
    /// Clear low-pass filtering for a mixer bus.
    /// </summary>
    public static void ClearBusLowPassFilter(AudioBus bus)
    {
        AudioAPI.SetBusLowPassGain((int)bus, 1.0f);
    }

    /// <summary>
    /// Get the current low-pass filter gain for a mixer bus.
    /// </summary>
    public static float GetBusLowPassFilter(AudioBus bus)
    {
        return AudioAPI.GetBusLowPassGain((int)bus);
    }

    /// <summary>
    /// Set the 3D audio listener position, velocity, and orientation.
    /// </summary>
    /// <param name="position">Listener position.</param>
    /// <param name="velocity">Listener velocity (for Doppler effect).</param>
    /// <param name="forward">Forward direction vector.</param>
    /// <param name="up">Up direction vector.</param>
    public static void SetListener(Vector3 position, Vector3 velocity, Vector3 forward, Vector3 up)
        => AudioAPI.SetListener(position.X, position.Y, position.Z, velocity.X, velocity.Y, velocity.Z, forward.X, forward.Y, forward.Z, up.X, up.Y, up.Z);
}

