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
    public void SetPosition(Vector3 position, Vector3 velocity) => Audio.SetInstancePosition(this, position, velocity);
    public void Stop(StopMode mode = StopMode.Immediate) => Audio.Stop(this, mode);
}

/// <summary>
/// Audio playback policy for single-instance sounds.
/// </summary>
public enum PlayPolicy
{
    KeepOldest = 0,
    KeepNewest = 1,
    RestartSingle = 2
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
    public static AudioHandle PlaySingle(string cueId, PlayPolicy policy = PlayPolicy.KeepOldest, float volume = 1.0f, float pitch = 1.0f)
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
    /// Set the 3D audio listener position, velocity, and orientation.
    /// </summary>
    /// <param name="position">Listener position.</param>
    /// <param name="velocity">Listener velocity (for Doppler effect).</param>
    /// <param name="forward">Forward direction vector.</param>
    /// <param name="up">Up direction vector.</param>
    public static void SetListener(Vector3 position, Vector3 velocity, Vector3 forward, Vector3 up)
        => AudioAPI.SetListener(position.X, position.Y, position.Z, velocity.X, velocity.Y, velocity.Z, forward.X, forward.Y, forward.Z, up.X, up.Y, up.Z);
}

