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
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SOUNDTYPES_H
#define SOUNDTYPES_H

#include <cstdint>

namespace Audio {
    enum class Bus : uint8_t {
        Master = 0,
        Music,
        SFX,
        UI,
        Ambient,
        Count
    };

    // playmode choice
    enum class PlayMode { Single, Looping };
    // to stop an instance
    enum class StopMode { Immediate, AllowFadeOut };

    struct SoundParams {
        // Static cue data (loaded once when created the Fmod::sound)
        bool   Stream = false;      // true when create as streaming (good for bgms)
        float  DefaultVolume = 1.0f; //used for UI
        bool   Is3D = false;         // treat sounds as 3D (listener/position affect it)
    };

    struct PlaySettings {
        // per-instance overrides apply when calling play()
        PlayMode Mode = PlayMode::Single; //desired behaviour at the start
        float    Volume = 1.0f;   // 0..1 linear gain
        float    Pitch = 1.0f;    // 0.5..2.0 etc frequency ratio
        bool     Loop = false;    // convenience for low-level APIs
        bool     Spatial3D = false; // true for 3D positional sound
        float    Pan = 0.0f;      // -1 = left, 0 = center, 1 = right (2D only)
        bool     StartPaused = false;
    };
    // Minimal vector type to avoid dragging in engine math for audio-only callers.
    struct Vec3 { float x{}, y{}, z{}; };

    // Aggregated listener parameters for 3D audio.
    struct ListenerParams {
        Vec3 Position{};        // world-space position of the listener
        Vec3 Forward{ 0,0,-1 }; // listener facing direction
        Vec3 Up{ 0,1,0 };       // listener up vector
        Vec3 Velocity{};        // used for Doppler
    };
}

#endif
