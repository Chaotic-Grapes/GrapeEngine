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
* - FMOD Core Programmer�s Guide (System, Sound, Channel, ChannelGroup usage).
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
#include "Export.h"

namespace Audio {
    // Opaque handle for a playing sound instance (maps to FMOD::Channel).
    struct PlaybackHandle {
        uint64_t Id = 0;                                    // Means invalid
        explicit operator bool() const { return Id != 0; }  // allow if handle
        bool operator==(const PlaybackHandle& o) const { return Id == o.Id; }
    };
   
    // Struct for cache to keep loaded path that hit previously
    struct CueEntry {
        FMOD::Sound* Sound = nullptr;
        SoundParams  Params{};
        std::string  SourcePath;   // NEW: keep original file path (for display / reload)
    };

    // Policy for PlaySingle behavior
    enum class PlayPolicy {
        NewInstance,             // do not enforce single-instance
        SingleInstanceRestart,   // if playing, restart from time 0 on same channel
        SingleInstanceResume,    // if paused, unpause; if playing, return same channel
        SingleInstanceIgnore     // if already playing, do nothing (return existing)
    };

    class GRAPEENGINE_API FmodAudioDevice final {
    public:
        FmodAudioDevice() = default; // POD-like; call Initialize() before use

        // Lifecycle
        bool Initialize();
        void Update();    // pumps FMOD and prunes stale singletons
        void Shutdown();

        // Cues
        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);
        void UnloadCue(const std::string& cueId);
        bool HasCue(const std::string& cueId) const;

        // Playback
        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings);
        void Stop(PlaybackHandle handle, StopMode mode);

        // Single-instance helpers (stop stacking, make Stop reliable by cue)
        PlaybackHandle PlaySingle(const std::string& cueId,
            const PlaySettings& settings,
            PlayPolicy policy = PlayPolicy::SingleInstanceRestart);
        void StopCue(const std::string& cueId, StopMode mode);
        bool IsCuePlaying(const std::string& cueId) const;

        // Per-instance controls
        void SetInstanceVolume(PlaybackHandle handle, float volume);
        void SetInstancePitch(PlaybackHandle handle, float pitch);
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel);

        // Listener and master
        void SetListener(const ListenerParams& listener);
        void SetMasterVolume(float volume);
        float GetMasterVolume() const;

        // Pause/Resume all audio
        void PauseAll();
        void ResumeAll();

        // Introspection
        void GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const;

    private:
        // FMOD objects
        FMOD::System* m_system = nullptr;
        FMOD::ChannelGroup* m_master = nullptr;
        float               m_masterVolume = 1.0f;

        // Maps
        std::unordered_map<uint64_t, FMOD::Channel*> m_channels;  // active channels keyed by handle id
        std::unordered_map<std::string, CueEntry>    m_cues;      // loaded sounds keyed by cueId

        // Single-instance bookkeeping: cueId -> last active handle id
        std::unordered_map<std::string, uint64_t>    m_activeByCue;

        // Handle ids
        uint64_t m_nextId = 1;

        // Helpers
        FMOD::Sound* _getOrCreateSound(const std::string& cueId,
            const std::string& path,
            const SoundParams& params);
        FMOD::Channel* _channelFromHandle(PlaybackHandle h);
        FMOD::Sound* _createSoundFromMemory(const std::string& cueId,
            const std::string& path,
            const SoundParams& params);
    };

} // namespace Audio

extern Audio::FmodAudioDevice* gAudioDevice;

#endif
