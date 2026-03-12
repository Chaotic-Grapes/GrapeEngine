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
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/


#ifndef FMODAUDIODEVICE_H
#define FMODAUDIODEVICE_H

#include <array>
#include <unordered_map>
#include <string>
#include <fmod.hpp>
#include "audio/SoundTypes.h"
#include "Export.h"

namespace Audio {
    // Opaque ID used by gameplay code to reference an active FMOD channel.
    struct PlaybackHandle {
        uint64_t Id = 0;                                    // 0 means "invalid / no playback"
        explicit operator bool() const { return Id != 0; }  // true when a valid handle was returned
        bool operator==(const PlaybackHandle& o) const { return Id == o.Id; }
    };
   
    // Cached cue data: FMOD sound object + original settings + source path.
    struct CueEntry {
        FMOD::Sound* Sound = nullptr;
        SoundParams  Params{};
        std::string  SourcePath;   // Preserved for debug display and device-reload recovery.
    };

    // Policy used by PlaySingle when the same cue is already active.
    enum class PlayPolicy {
        NewInstance,             // do not enforce single-instance
        SingleInstanceRestart,   // if playing, restart from time 0 on same channel
        SingleInstanceResume,    // if paused, unpause; if playing, return same channel
        SingleInstanceIgnore     // if already playing, do nothing (return existing)
    };

    class GRAPEENGINE_API FmodAudioDevice final {
    public:
        FmodAudioDevice() { m_busLowPassGain.fill(1.0f); } // POD-like; call Initialize() before use

        // Lifecycle
        bool Initialize();
        bool InitializeWithDevice(const std::string& deviceID);  // Initialize with specific device
        void Update();    // pumps FMOD and prunes stale singletons
        void Shutdown();

        // Cues
        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);
        void UnloadCue(const std::string& cueId);
        bool HasCue(const std::string& cueId) const;

        // Playback
        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings, Bus bus = Bus::SFX);
        void Stop(PlaybackHandle handle, StopMode mode);

        // Single-instance helpers (stop stacking, make Stop reliable by cue)
        PlaybackHandle PlaySingle(const std::string& cueId,
            const PlaySettings& settings,
            PlayPolicy policy = PlayPolicy::SingleInstanceRestart,
            Bus bus = Bus::SFX);
        void StopCue(const std::string& cueId, StopMode mode);
        bool IsCuePlaying(const std::string& cueId) const;

        // Per-instance controls
        void SetInstanceVolume(PlaybackHandle handle, float volume);
        void SetInstancePitch(PlaybackHandle handle, float pitch);
        void SetInstancePan(PlaybackHandle handle, float pan);
        void SetInstanceLowPassGain(PlaybackHandle handle, float gain);
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel);
        bool IsHandlePlaying(PlaybackHandle handle) const;

        // Bus-level controls routed through FMOD channel groups and DSPs.
        void SetBusLowPassGain(Bus bus, float gain);
        float GetBusLowPassGain(Bus bus) const;

        // Listener and master
        void SetListener(const ListenerParams& listener);
        void SetMasterVolume(float volume);
        float GetMasterVolume() const;

        // Pause/Resume all audio
        void PauseAll();
        void ResumeAll();

        // Introspection / low-level FMOD access for systems that need graph setup.
        void GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const;
        // Raw FMOD System pointer. Non-owning; valid only while device is initialized.
        FMOD::System* GetSystem() const { return m_system; }
        // Raw FMOD master channel group pointer. Non-owning; valid only while initialized.
        FMOD::ChannelGroup* GetMasterChannelGroup() const { return m_master; }

    private:
        static constexpr size_t kBusCount = static_cast<size_t>(Bus::Count);

        // FMOD objects
        FMOD::System* m_system = nullptr;
        FMOD::ChannelGroup* m_master = nullptr;
        float               m_masterVolume = 1.0f;
        std::array<FMOD::ChannelGroup*, kBusCount> m_busGroups{};
        std::array<FMOD::DSP*, kBusCount> m_busLowPassDsps{};
        std::array<float, kBusCount> m_busLowPassGain{};

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
        bool _initializeBusRouting();
        void _shutdownBusRouting();
        FMOD::ChannelGroup* _channelGroupForBus(Bus bus) const;
        void _applyBusLowPassGain(Bus bus);
        static float _clampLowPassGain(float gain);
        static float _lowPassGainToCutoffHz(float gain);
    };

} // namespace Audio

extern Audio::FmodAudioDevice* gAudioDevice;

#endif
