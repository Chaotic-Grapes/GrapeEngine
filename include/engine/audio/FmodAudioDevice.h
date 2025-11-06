/*
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file FmodAudioDevice.h
* @brief Thin FMOD Core wrapper: cue loading, instance playback, simple 3D controls.
*
* @details
* Exposes a minimal device interface used by game systems:
* - Initialize/Update/Shutdown: lifecycle of the FMOD::System
* - Cue management: LoadCue/UnloadCue/HasCue (by string ID)
* - Playback: Play returns a small PlaybackHandle that identifies a channel
* - Controls: Stop/SetInstanceVolume/SetInstancePitch/SetInstancePosition
* - Listener: SetListener to update 3D listener attributes
* - Master volume: global gain control cached locally for UI/serialization
* Internally caches FMOD::Sound objects per cue and active FMOD::Channel per handle.
*
* @sources
* - FMOD Core Programmer’s Guide (System, Sound, Channel, ChannelGroup usage).
*
* @dependencies
* - <fmod.hpp>, <unordered_map>, <string>, and Audio::SoundTypes
* 
* Copyright (C) 2025 DigiPen Institute of Technology.
* Reproduction or disclosure of this file or its contents without the
* 
*/

#ifndef FMODAUDIODEVICE_H
#define FMODAUDIODEVICE_H

#include <unordered_map>
#include <string>
#include <fmod.hpp>
#include "audio/SoundTypes.h"

namespace Audio {
    // Opaque handle for a playing sound instance (maps to FMOD::Channel).
    struct PlaybackHandle {
        uint64_t Id = 0;                                    // Means invalid
        explicit operator bool() const { return Id != 0; }  // allow if handle
        bool operator==(const PlaybackHandle& o) const { return Id == o.Id; }
    };
   
    // Loaded cue entry: FMOD::Sound plus the creation params used.
    struct CueEntry {
        FMOD::Sound* Sound = nullptr; // owned by this device; released on Shutdown/UnloadCue
        SoundParams Params{}; // remembers how the sound was created (stream/3D)
    };

    class FmodAudioDevice final {
    public:
        FmodAudioDevice() = default; // POD-like; call Initialize() before use


        bool Initialize(); // create FMOD system and master group
        void Update(); // pump FMOD mixer and async tasks
        void Shutdown(); // release sounds/channels/system


        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params); // create or reuse sound
        void UnloadCue(const std::string& cueId); // release sound by id if loaded
        bool HasCue(const std::string& cueId) const; // query existence of a cue


        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings); // start playing a sound
        void Stop(PlaybackHandle handle, StopMode mode); // stop channel immediately or allow fade
        void SetInstanceVolume(PlaybackHandle handle, float volume); // per-channel volume
        void SetInstancePitch(PlaybackHandle handle, float pitch); // per-channel pitch


        void SetListener(const ListenerParams& listener); // update 3D listener state
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel); // 3D attributes


        void SetMasterVolume(float volume); // clamp 0..1 and apply to FMOD master group
        float GetMasterVolume() const; // return cached value


    private:
        // FMOD objects
        FMOD::System* m_system = nullptr; // created in Initialize(), released in Shutdown()
        FMOD::ChannelGroup* m_master = nullptr; // master channel group for global volume
        float m_masterVolume = 1.0f; // cached master volume (UI friendly)


        // Book-keeping
        std::unordered_map<uint64_t, FMOD::Channel*> m_channels; // active instances
        std::unordered_map<std::string, CueEntry> m_cues; // loaded sounds per cue id
        uint64_t m_nextId = 1; // monotonically increasing instance ids


        // Helpers
        FMOD::Sound* _getOrCreateSound(const std::string& cueId, const std::string& path, const SoundParams& params); // loads and caches
        FMOD::Channel* _channelFromHandle(PlaybackHandle h); // map handle to channel pointer (or nullptr)
    };


}


#endif
