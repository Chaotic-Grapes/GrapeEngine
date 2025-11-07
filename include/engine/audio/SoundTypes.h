/*
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file SoundTypes.h
* @brief Common audio enums and POD parameter structs shared by the audio layer.
*
* @details
* This header defines lightweight types used by the FMOD-backed audio device:
* - PlayMode / StopMode: simple policies controlling playback and stopping.
* - SoundParams: immutable cue properties (streaming, default volume, 2D/3D).
* - PlaySettings: per-play call overrides (loop, volume, pitch).
* - Vec3 / ListenerParams: minimal 3D vector and listener state for 3D audio.
* All types are POD/trivially copyable so they can be stored in ECS components.
*
* @sources
* - FMOD Core API concepts (looping, 2D vs 3D sounds, listener attributes).
*
* @dependencies
* - None (only plain C++ types).
*
* Copyright (C) 2025 DigiPen Institute of Technology.
* Reproduction or disclosure of this file or its contents without the
*
*/

#ifndef SOUNDTYPES_H
#define SOUNDTYPES_H

namespace Audio {
    // playmode choice
    enum class PlayMode { Single, Looping };
    // to stop an instance
    enum class StopMode { Immediate, AllowFadeOut };

    struct SoundParams {
        // Static cue data (loaded once when created the Fmod::sound)
        bool   stream = false;      // true when create as streaming (good for bgms)
        float  defaultVolume = 1.0f; //used for UI
        bool   is3D = false;         // treat sounds as 3D (listener/position affect it)
    };

    struct PlaySettings {
        // per-instance overrides apply when calling play()
        PlayMode mode = PlayMode::Single; //desired behaviour at the start
        float    volume = 1.0f;   // 0..1 linear gain
        float    pitch = 1.0f;    // 0.5..2.0 etc frequency ratio
        bool     loop = false;    // convenience for low-level APIs
    };
    // Minimal vector type to avoid dragging in engine math for audio-only callers.
    struct Vec3 { float x{}, y{}, z{}; };

    // Aggregated listener parameters for 3D audio.
        struct ListenerParams {
        Vec3 position{};        // world-space position of the listener
        Vec3 forward{ 0,0,-1 }; // listener facing direction
        Vec3 up{ 0,1,0 };       // listener up vector
        Vec3 velocity{};        // used for Doppler
    };
}

#endif
