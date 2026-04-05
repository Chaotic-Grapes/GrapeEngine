/* Start Header *****************************************************************/
/*!
\file    Interop_Audio.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    21st November 2025
\brief
C API exports for managed C# scripting systems for audio playback and control.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "audio/FmodAudioDevice.h"
#include "audio/AudioEngine.h"
#include "core/Logger.h"

// External audio device reference
extern Audio::FmodAudioDevice* gAudioDevice;
extern Audio::AudioEngine* Audio::gAudioEngine;

// ============================================================================
// Audio API - Sound Loading and Playback
// ============================================================================

/**
 * @brief Load an audio cue from a file path
 * @param cueId The identifier for the cue
 * @param filePath The file path to the audio asset
 * @param is3D Whether the sound should be treated as 3D
 * @param isLooping Whether the sound should loop
 * @param isStreaming Whether the sound should be streamed
 * @return True if the cue was loaded successfully, false otherwise
 */
INTEROP_API bool EngineInterop_Audio_LoadCue(const char* cueId, const char* filePath, bool is3D, bool isLooping, bool isStreaming) {
    (void)isLooping; // Currently unused
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return false;
    }

    Audio::SoundParams params;
    params.Is3D = is3D;
    params.Stream = isStreaming;

    return Audio::gAudioEngine->LoadCue(cueId, filePath, params);
}

/**
 * @brief Unload a previously loaded audio cue
 * @param cueId The identifier for the cue to unload
 */
INTEROP_API void EngineInterop_Audio_UnloadCue(const char* cueId) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::gAudioEngine->UnloadCue(cueId);
}

/**
 * @brief Check if a cue is loaded
 * @param cueId The identifier for the cue
 * @return True if the cue is loaded, false otherwise
 */
INTEROP_API bool EngineInterop_Audio_HasCue(const char* cueId) {
    if (!Audio::gAudioEngine) {
        return false;
    }

    return Audio::gAudioEngine->HasCue(cueId);
}

/**
 * @brief Play a sound cue
 * @param cueId The identifier for the cue to play
 * @param volume The volume for playback (0.0 to 1.0)
 * @param pitch The pitch for playback (0.5 to 2.0)
 * @param paused Whether to start the sound paused
 * @return A handle ID for the playback instance
 */
INTEROP_API uint64_t EngineInterop_Audio_Play(const char* cueId, float volume, float pitch, bool paused) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 0;
    }

    Audio::PlaySettings settings;
    settings.Volume = volume;
    settings.Pitch = pitch;
    settings.Loop = false;
    settings.StartPaused = paused;

    Audio::PlaybackHandle handle = Audio::gAudioEngine->Play(cueId, settings, Audio::Bus::SFX);
    return handle.Id;
}

/**
 * @brief Play a sound cue with single-instance policy
 * @param cueId The identifier for the cue to play
 * @param volume The volume for playback (0.0 to 1.0)
 * @param pitch The pitch for playback (0.5 to 2.0)
 * @param policy The play policy (0=NewInstance, 1=SingleInstanceRestart, 2=SingleInstanceResume, 3=SingleInstanceIgnore)
 * @return A handle ID for the playback instance
 */
INTEROP_API uint64_t EngineInterop_Audio_PlaySingle(const char* cueId, float volume, float pitch, int policy) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 0;
    }

    Audio::PlaySettings settings;
    settings.Volume = volume;
    settings.Pitch = pitch;

    Audio::PlayPolicy playPolicy = static_cast<Audio::PlayPolicy>(policy);
    Audio::PlaybackHandle handle = Audio::gAudioEngine->PlaySingle(cueId, settings, playPolicy, Audio::Bus::SFX);
    return handle.Id;
}

/**
 * @brief Stop a playing sound instance
 * @param handleId The handle ID of the playback instance to stop
 * @param stopMode The stop mode (0=Immediate, 1=AllowFadeOut)
 */
INTEROP_API void EngineInterop_Audio_Stop(uint64_t handleId, int stopMode) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::StopMode mode = static_cast<Audio::StopMode>(stopMode);

    Audio::gAudioEngine->Stop(handle, mode);
}

/**
 * @brief Stop all instances of a cue
 * @param cueId The identifier for the cue to stop
 * @param stopMode The stop mode (0=Immediate, 1=AllowFadeOut)
 */
INTEROP_API void EngineInterop_Audio_StopCue(const char* cueId, int stopMode) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::StopMode mode = static_cast<Audio::StopMode>(stopMode);
    Audio::gAudioEngine->StopCue(cueId, mode);
}

/**
 * @brief Check if a cue is currently playing
 * @param cueId The identifier for the cue
 * @return True if the cue is playing, false otherwise
 */
INTEROP_API bool EngineInterop_Audio_IsCuePlaying(const char* cueId) {
    if (!Audio::gAudioEngine) {
        return false;
    }

    return Audio::gAudioEngine->IsCuePlaying(cueId);
}

// ============================================================================
// Audio API - Playback Control
// ============================================================================

/**
 * @brief Set the volume of a playing sound instance
 * @param handleId The handle ID of the playback instance
 * @param volume The new volume (0.0 to 1.0)
 */
INTEROP_API void EngineInterop_Audio_SetInstanceVolume(uint64_t handleId, float volume) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstanceVolume(handle, volume);
}

/**
 * @brief Set the pitch of a playing sound instance
 * @param handleId The handle ID of the playback instance
 * @param pitch The new pitch (0.5 to 2.0)
 */
INTEROP_API void EngineInterop_Audio_SetInstancePitch(uint64_t handleId, float pitch) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstancePitch(handle, pitch);
}

/**
 * @brief Set the stereo pan of a playing sound instance (2D only)
 * @param handleId The handle ID of the playback instance
 * @param pan The pan value (-1.0 left to 1.0 right)
 */
INTEROP_API void EngineInterop_Audio_SetInstancePan(uint64_t handleId, float pan) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstancePan(handle, pan);
}

/**
 * @brief Set the low-pass gain of a playing sound instance
 * @param handleId The handle ID of the playback instance
 * @param gain The low-pass gain (0.0 = strongest low-pass, 1.0 = bypass)
 */
INTEROP_API void EngineInterop_Audio_SetInstanceLowPassGain(uint64_t handleId, float gain) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstanceLowPassGain(handle, gain);
}

/**
 * @brief Set the 3D position and velocity of a playing sound instance
 * @param handleId The handle ID of the playback instance
 * @param posX The X position
 * @param posY The Y position
 * @param posZ The Z position
 * @param velX The X velocity
 * @param velY The Y velocity
 * @param velZ The Z velocity
 */
INTEROP_API void EngineInterop_Audio_SetInstancePosition(uint64_t handleId, float posX, float posY, float posZ, float velX, float velY, float velZ) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    
    Audio::Vec3 position = { posX, posY, posZ };
    Audio::Vec3 velocity = { velX, velY, velZ };
    
    Audio::gAudioEngine->SetInstancePosition(handle, position, velocity);
}

/**
 * @brief Set the 3D minimum and maximum distances for a playing sound instance.
 * @param handleId The handle ID of the playback instance.
 * @param minDistance Distance at which the sound is at full volume.
 * @param maxDistance Distance at which the sound is at minimum volume.
 */
INTEROP_API void EngineInterop_Audio_SetInstance3DMinMaxDistance(uint64_t handleId, float minDistance, float maxDistance) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstance3DMinMaxDistance(handle, minDistance, maxDistance);
}

/**
 * @brief Get the 3D minimum distance for a playing sound instance.
 * @param handleId The handle ID of the playback instance.
 * @return Minimum distance value (default 1.0f if unavailable).
 */
INTEROP_API float EngineInterop_Audio_GetInstance3DMinDistance(uint64_t handleId) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    return Audio::gAudioEngine->GetInstance3DMinDistance(handle);
}

/**
 * @brief Get the 3D maximum distance for a playing sound instance.
 * @param handleId The handle ID of the playback instance.
 * @return Maximum distance value (default 25.0f if unavailable).
 */
INTEROP_API float EngineInterop_Audio_GetInstance3DMaxDistance(uint64_t handleId) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 25.0f;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    return Audio::gAudioEngine->GetInstance3DMaxDistance(handle);
}

INTEROP_API void EngineInterop_Audio_SetInstance3DSpread(uint64_t handleId, float spread) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstance3DSpread(handle, spread);
}

INTEROP_API float EngineInterop_Audio_GetInstance3DSpread(uint64_t handleId) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 0.0f;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    return Audio::gAudioEngine->GetInstance3DSpread(handle);
}

INTEROP_API void EngineInterop_Audio_SetInstance3DLevel(uint64_t handleId, float level) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::gAudioEngine->SetInstance3DLevel(handle, level);
}

INTEROP_API float EngineInterop_Audio_GetInstance3DLevel(uint64_t handleId) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    return Audio::gAudioEngine->GetInstance3DLevel(handle);
}

INTEROP_API void EngineInterop_Audio_SetDefault3DMinMaxDistance(float minDistance, float maxDistance) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->SetDefault3DMinMaxDistance(minDistance, maxDistance);
}

INTEROP_API float EngineInterop_Audio_GetDefault3DMinDistance() {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }
    return Audio::gAudioEngine->GetDefault3DMinDistance();
}

INTEROP_API float EngineInterop_Audio_GetDefault3DMaxDistance() {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 25.0f;
    }
    return Audio::gAudioEngine->GetDefault3DMaxDistance();
}

INTEROP_API void EngineInterop_Audio_SetDefault3DSpread(float spread) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->SetDefault3DSpread(spread);
}

INTEROP_API float EngineInterop_Audio_GetDefault3DSpread() {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 0.0f;
    }
    return Audio::gAudioEngine->GetDefault3DSpread();
}

INTEROP_API void EngineInterop_Audio_SetDefault3DLevel(float level) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->SetDefault3DLevel(level);
}

INTEROP_API float EngineInterop_Audio_GetDefault3DLevel() {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }
    return Audio::gAudioEngine->GetDefault3DLevel();
}

// ============================================================================
// Audio API - Master Controls
// ============================================================================

/**
 * @brief Set the master volume
 * @param volume The new master volume (0.0 to 1.0)
 */
INTEROP_API void EngineInterop_Audio_SetMasterVolume(float volume) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::gAudioEngine->SetBusVolume(Audio::Bus::Master, volume);
}

/**
 * @brief Get the master volume
 * @return The current master volume (0.0 to 1.0)
 */
INTEROP_API float EngineInterop_Audio_GetMasterVolume() {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }

    return Audio::gAudioEngine->GetBusVolume(Audio::Bus::Master);
}

/**
 * @brief Set the listener parameters for 3D audio
 * @param posX The X position of the listener
 * @param posY The Y position of the listener
 * @param posZ The Z position of the listener
 * @param velX The X velocity of the listener
 * @param velY The Y velocity of the listener
 * @param velZ The Z velocity of the listener
 * @param fwdX The X component of the forward vector
 * @param fwdY The Y component of the forward vector
 * @param fwdZ The Z component of the forward vector
 * @param upX The X component of the up vector
 * @param upY The Y component of the up vector
 * @param upZ The Z component of the up vector
 */
INTEROP_API void EngineInterop_Audio_SetListener(float posX, float posY, float posZ, 
                                             float velX, float velY, float velZ,
                                             float fwdX, float fwdY, float fwdZ,
                                             float upX, float upY, float upZ) {
    if (!Audio::gAudioEngine) {
          std::cerr << "[ScriptAPI] Audio device not initialized" << '\n';
        return;
    }

    Audio::ListenerParams listener;
    listener.Position = { posX, posY, posZ };
    listener.Velocity = { velX, velY, velZ };
    listener.Forward = { fwdX, fwdY, fwdZ };
    listener.Up = { upX, upY, upZ };

    Audio::gAudioEngine->SetListener(listener);
}

// ============================================================================
// Audio API - Bus Controls
// ============================================================================

/**
 * @brief Set a mixer bus volume
 * @param bus Bus index
 * @param volume New volume (0.0 to 1.0)
 */
INTEROP_API void EngineInterop_Audio_SetBusVolume(int bus, float volume) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->SetBusVolume(static_cast<Audio::Bus>(bus), volume);
}

/**
 * @brief Get a mixer bus volume
 * @param bus Bus index
 * @return Current volume
 */
INTEROP_API float EngineInterop_Audio_GetBusVolume(int bus) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }
    return Audio::gAudioEngine->GetBusVolume(static_cast<Audio::Bus>(bus));
}

/**
 * @brief Fade a mixer bus to a target volume
 * @param bus Bus index
 * @param targetVolume Target volume
 * @param duration Fade duration in seconds
 */
INTEROP_API void EngineInterop_Audio_FadeBusVolume(int bus, float targetVolume, float duration) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->FadeBusVolume(static_cast<Audio::Bus>(bus), targetVolume, duration);
}

/**
 * @brief Set a mixer bus low-pass gain
 * @param bus Bus index
 * @param gain Low-pass gain (1.0 = no filtering, 0.0 = strongest filtering)
 */
INTEROP_API void EngineInterop_Audio_SetBusLowPassGain(int bus, float gain) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->SetBusLowPassGain(static_cast<Audio::Bus>(bus), gain);
}

/**
 * @brief Get a mixer bus low-pass gain
 * @param bus Bus index
 * @return Current low-pass gain
 */
INTEROP_API float EngineInterop_Audio_GetBusLowPassGain(int bus) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }
    return Audio::gAudioEngine->GetBusLowPassGain(static_cast<Audio::Bus>(bus));
}

INTEROP_API void EngineInterop_Audio_SetBusLowPassResonance(int bus, float resonance) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }
    Audio::gAudioEngine->SetBusLowPassResonance(static_cast<Audio::Bus>(bus), resonance);
}

INTEROP_API float EngineInterop_Audio_GetBusLowPassResonance(int bus) {
    if (!Audio::gAudioEngine) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }
    return Audio::gAudioEngine->GetBusLowPassResonance(static_cast<Audio::Bus>(bus));
}
