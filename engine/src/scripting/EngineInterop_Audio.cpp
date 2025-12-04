/* Start Header *****************************************************************/
/*!
\file    EngineInterop_Audio.cpp
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

#include "audio/FmodAudioDevice.h"
#include "core/Logger.h"

// Export macro for C API
#ifdef _WIN32
    #ifdef BUILDING_ENGINE_INTEROP
        #define ENGINE_INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define ENGINE_INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define ENGINE_INTEROP_API extern "C"
#endif

// External audio device reference
extern Audio::FmodAudioDevice* gAudioDevice;

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
ENGINE_INTEROP_API bool EngineInterop_Audio_LoadCue(const char* cueId, const char* filePath, bool is3D, bool isLooping, bool isStreaming) {
    (void)isLooping; // Currently unused
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return false;
    }

    Audio::SoundParams params;
    params.Is3D = is3D;
    params.Stream = isStreaming;

    return gAudioDevice->LoadCue(cueId, filePath, params);
}

/**
 * @brief Unload a previously loaded audio cue
 * @param cueId The identifier for the cue to unload
 */
ENGINE_INTEROP_API void EngineInterop_Audio_UnloadCue(const char* cueId) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    gAudioDevice->UnloadCue(cueId);
}

/**
 * @brief Check if a cue is loaded
 * @param cueId The identifier for the cue
 * @return True if the cue is loaded, false otherwise
 */
ENGINE_INTEROP_API bool EngineInterop_Audio_HasCue(const char* cueId) {
    if (!gAudioDevice) {
        return false;
    }

    return gAudioDevice->HasCue(cueId);
}

/**
 * @brief Play a sound cue
 * @param cueId The identifier for the cue to play
 * @param volume The volume for playback (0.0 to 1.0)
 * @param pitch The pitch for playback (0.5 to 2.0)
 * @param paused Whether to start the sound paused
 * @return A handle ID for the playback instance
 */
ENGINE_INTEROP_API uint64_t EngineInterop_Audio_Play(const char* cueId, float volume, float pitch, bool paused) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 0;
    }

    Audio::PlaySettings settings;
    settings.Volume = volume;
    settings.Pitch = pitch;
    settings.Loop = paused;

    Audio::PlaybackHandle handle = gAudioDevice->Play(cueId, settings);
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
ENGINE_INTEROP_API uint64_t EngineInterop_Audio_PlaySingle(const char* cueId, float volume, float pitch, int policy) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 0;
    }

    Audio::PlaySettings settings;
    settings.Volume = volume;
    settings.Pitch = pitch;

    Audio::PlayPolicy playPolicy = static_cast<Audio::PlayPolicy>(policy);
    Audio::PlaybackHandle handle = gAudioDevice->PlaySingle(cueId, settings, playPolicy);
    return handle.Id;
}

/**
 * @brief Stop a playing sound instance
 * @param handleId The handle ID of the playback instance to stop
 * @param stopMode The stop mode (0=Immediate, 1=AllowFadeOut)
 */
ENGINE_INTEROP_API void EngineInterop_Audio_Stop(uint64_t handleId, int stopMode) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    Audio::StopMode mode = static_cast<Audio::StopMode>(stopMode);

    gAudioDevice->Stop(handle, mode);
}

/**
 * @brief Stop all instances of a cue
 * @param cueId The identifier for the cue to stop
 * @param stopMode The stop mode (0=Immediate, 1=AllowFadeOut)
 */
ENGINE_INTEROP_API void EngineInterop_Audio_StopCue(const char* cueId, int stopMode) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::StopMode mode = static_cast<Audio::StopMode>(stopMode);
    gAudioDevice->StopCue(cueId, mode);
}

/**
 * @brief Check if a cue is currently playing
 * @param cueId The identifier for the cue
 * @return True if the cue is playing, false otherwise
 */
ENGINE_INTEROP_API bool EngineInterop_Audio_IsCuePlaying(const char* cueId) {
    if (!gAudioDevice) {
        return false;
    }

    return gAudioDevice->IsCuePlaying(cueId);
}

// ============================================================================
// Audio API - Playback Control
// ============================================================================

/**
 * @brief Set the volume of a playing sound instance
 * @param handleId The handle ID of the playback instance
 * @param volume The new volume (0.0 to 1.0)
 */
ENGINE_INTEROP_API void EngineInterop_Audio_SetInstanceVolume(uint64_t handleId, float volume) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    gAudioDevice->SetInstanceVolume(handle, volume);
}

/**
 * @brief Set the pitch of a playing sound instance
 * @param handleId The handle ID of the playback instance
 * @param pitch The new pitch (0.5 to 2.0)
 */
ENGINE_INTEROP_API void EngineInterop_Audio_SetInstancePitch(uint64_t handleId, float pitch) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    gAudioDevice->SetInstancePitch(handle, pitch);
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
ENGINE_INTEROP_API void EngineInterop_Audio_SetInstancePosition(uint64_t handleId, float posX, float posY, float posZ, float velX, float velY, float velZ) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    Audio::PlaybackHandle handle;
    handle.Id = handleId;
    
    Audio::Vec3 position = { posX, posY, posZ };
    Audio::Vec3 velocity = { velX, velY, velZ };
    
    gAudioDevice->SetInstancePosition(handle, position, velocity);
}

// ============================================================================
// Audio API - Master Controls
// ============================================================================

/**
 * @brief Set the master volume
 * @param volume The new master volume (0.0 to 1.0)
 */
ENGINE_INTEROP_API void EngineInterop_Audio_SetMasterVolume(float volume) {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return;
    }

    gAudioDevice->SetMasterVolume(volume);
}

/**
 * @brief Get the master volume
 * @return The current master volume (0.0 to 1.0)
 */
ENGINE_INTEROP_API float EngineInterop_Audio_GetMasterVolume() {
    if (!gAudioDevice) {
        LOG_ERROR("[ScriptAPI] Audio device not initialized");
        return 1.0f;
    }

    return gAudioDevice->GetMasterVolume();
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
ENGINE_INTEROP_API void EngineInterop_Audio_SetListener(float posX, float posY, float posZ, 
                                             float velX, float velY, float velZ,
                                             float fwdX, float fwdY, float fwdZ,
                                             float upX, float upY, float upZ) {
    if (!gAudioDevice) {
          std::cerr << "[ScriptAPI] Audio device not initialized" << '\n';
        return;
    }

    Audio::ListenerParams listener;
    listener.Position = { posX, posY, posZ };
    listener.Velocity = { velX, velY, velZ };
    listener.Forward = { fwdX, fwdY, fwdZ };
    listener.Up = { upX, upY, upZ };

    gAudioDevice->SetListener(listener);
}
