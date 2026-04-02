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
        FmodAudioDevice() {
            m_busLowPassGain.fill(1.0f);
            m_busLowPassResonance.fill(1.0f);
        } // POD-like; call Initialize() before use

        /**
         * @brief Initialize FMOD system using default output device.
         * @return True when initialization succeeds.
         */
        bool Initialize();

        /**
         * @brief Initialize FMOD system using a specific output device id.
         * @param deviceID Device identifier string.
         * @return True when initialization succeeds.
         */
        bool InitializeWithDevice(const std::string& deviceID);  // Initialize with specific device

        /**
         * @brief Pump FMOD and prune stale single-instance references.
         */
        void Update();    // pumps FMOD and prunes stale singletons

        /**
         * @brief Shutdown FMOD and release runtime resources.
         */
        void Shutdown();

        /**
         * @brief Load a cue into FMOD cache.
         * @param cueId Logical cue identifier.
         * @param filePath Source audio path.
         * @param params Sound loading parameters.
         * @return True when loading succeeds.
         */
        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);

        /**
         * @brief Unload one cue from cache.
         * @param cueId Logical cue identifier.
         */
        void UnloadCue(const std::string& cueId);

        /**
         * @brief Check whether cue id is currently loaded.
         * @param cueId Logical cue identifier.
         * @return True when cue exists.
         */
        bool HasCue(const std::string& cueId) const;

        /**
         * @brief Play one cue and return a handle for runtime control.
         * @param cueId Logical cue identifier.
         * @param settings Per-play settings.
         * @param bus Mixer bus route.
         * @return Playback handle for the started instance.
         */
        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings, Bus bus = Bus::SFX);

        /**
         * @brief Stop one playback handle.
         * @param handle Handle to stop.
         * @param mode Stop behavior.
         */
        void Stop(PlaybackHandle handle, StopMode mode);

        /**
         * @brief Play cue with single-instance policy control.
         * @param cueId Logical cue identifier.
         * @param settings Per-play settings.
         * @param policy Single-instance policy.
         * @param bus Mixer bus route.
         * @return Playback handle for active/started instance.
         */
        PlaybackHandle PlaySingle(const std::string& cueId,
            const PlaySettings& settings,
            PlayPolicy policy = PlayPolicy::SingleInstanceRestart,
            Bus bus = Bus::SFX);

        /**
         * @brief Stop cue playback via cue id mapping.
         * @param cueId Logical cue identifier.
         * @param mode Stop behavior.
         */
        void StopCue(const std::string& cueId, StopMode mode);

        /**
         * @brief Check if cue is actively playing.
         * @param cueId Logical cue identifier.
         * @return True when cue has active playback.
         */
        bool IsCuePlaying(const std::string& cueId) const;

        /**
         * @brief Set instance volume.
         * @param handle Playback handle.
         * @param volume Linear gain.
         */
        void SetInstanceVolume(PlaybackHandle handle, float volume);

        /**
         * @brief Set instance pitch.
         * @param handle Playback handle.
         * @param pitch Pitch multiplier.
         */
        void SetInstancePitch(PlaybackHandle handle, float pitch);

        /**
         * @brief Set instance stereo pan.
         * @param handle Playback handle.
         * @param pan Pan value in [-1, 1].
         */
        void SetInstancePan(PlaybackHandle handle, float pan);

        /**
         * @brief Set instance low-pass gain.
         * @param handle Playback handle.
         * @param gain Low-pass gain factor.
         */
        void SetInstanceLowPassGain(PlaybackHandle handle, float gain);

        /**
         * @brief Set instance 3D transform.
         * @param handle Playback handle.
         * @param pos World position.
         * @param vel World velocity.
         */
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel);
        void SetInstance3DMinMaxDistance(PlaybackHandle handle, float minDistance, float maxDistance);
        float GetInstance3DMinDistance(PlaybackHandle handle) const;
        float GetInstance3DMaxDistance(PlaybackHandle handle) const;
        void SetInstance3DSpread(PlaybackHandle handle, float spread);
        float GetInstance3DSpread(PlaybackHandle handle) const;
        void SetInstance3DLevel(PlaybackHandle handle, float level);
        float GetInstance3DLevel(PlaybackHandle handle) const;
        bool IsHandlePlaying(PlaybackHandle handle) const;

        // Default 3D settings applied to newly played Spatial3D instances.
        void SetDefault3DMinMaxDistance(float minDistance, float maxDistance);
        float GetDefault3DMinDistance() const;
        float GetDefault3DMaxDistance() const;
        void SetDefault3DSpread(float spread);
        float GetDefault3DSpread() const;
        void SetDefault3DLevel(float level);
        float GetDefault3DLevel() const;

        // Bus-level controls routed through FMOD channel groups and DSPs.
        void SetBusLowPassGain(Bus bus, float gain);

        /**
         * @brief Get bus low-pass gain.
         * @param bus Mixer bus.
         * @return Low-pass gain factor.
         */
        float GetBusLowPassGain(Bus bus) const;
        void SetBusLowPassResonance(Bus bus, float resonance);
        float GetBusLowPassResonance(Bus bus) const;

        /**
         * @brief Set listener transform attributes.
         * @param listener Listener payload.
         */
        void SetListener(const ListenerParams& listener);

        /**
         * @brief Set master volume.
         * @param volume Linear gain.
         */
        void SetMasterVolume(float volume);

        /**
         * @brief Get master volume.
         * @return Current master linear gain.
         */
        float GetMasterVolume() const;

        /**
         * @brief Pause all active playback.
         */
        void PauseAll();

        /**
         * @brief Resume all paused playback.
         */
        void ResumeAll();

        /**
         * @brief Get currently loaded cue id/path pairs.
         * @param out Output vector receiving cue id/path pairs.
         */
        void GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const;

        /**
         * @brief Access raw FMOD system pointer.
         * @return Non-owning FMOD system pointer, valid while initialized.
         */
        // Raw FMOD System pointer. Non-owning; valid only while device is initialized.
        FMOD::System* GetSystem() const { return m_system; }

        /**
         * @brief Access raw master channel group pointer.
         * @return Non-owning master group pointer, valid while initialized.
         */
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
        std::array<float, kBusCount> m_busLowPassResonance{};
        float m_default3DMinDistance = 1.0f;
        float m_default3DMaxDistance = 25.0f;
        float m_default3DSpread = 0.0f;
        float m_default3DLevel = 1.0f;

        // Maps
        std::unordered_map<uint64_t, FMOD::Channel*> m_channels;  // active channels keyed by handle id
        std::unordered_map<std::string, CueEntry>    m_cues;      // loaded sounds keyed by cueId

        // Single-instance bookkeeping: cueId -> last active handle id
        std::unordered_map<std::string, uint64_t>    m_activeByCue;

        // Handle ids
        uint64_t m_nextId = 1;

        /**
         * @brief Fetch cached sound or create it on demand.
         * @param cueId Logical cue identifier.
         * @param path Source audio path.
         * @param params Sound loading parameters.
         * @return FMOD sound pointer, or nullptr on failure.
         */
        FMOD::Sound* _getOrCreateSound(const std::string& cueId,
            const std::string& path,
            const SoundParams& params);

        /**
         * @brief Resolve a playback handle to FMOD channel.
         * @param h Playback handle.
         * @return Channel pointer, or nullptr when invalid/stale.
         */
        FMOD::Channel* _channelFromHandle(PlaybackHandle h);

        /**
         * @brief Create an FMOD sound object from input path.
         * @param cueId Logical cue identifier.
         * @param path Source audio path.
         * @param params Sound loading parameters.
         * @return Newly created sound pointer, or nullptr on failure.
         */
        FMOD::Sound* _createSoundFromMemory(const std::string& cueId,
            const std::string& path,
            const SoundParams& params);

        /**
         * @brief Initialize channel-group routing and bus DSP chain.
         * @return True when setup succeeds.
         */
        bool _initializeBusRouting();

        /**
         * @brief Tear down bus routing objects.
         */
        void _shutdownBusRouting();

        /**
         * @brief Resolve channel group pointer for a bus.
         * @param bus Mixer bus.
         * @return Channel group pointer, or nullptr when unavailable.
         */
        FMOD::ChannelGroup* _channelGroupForBus(Bus bus) const;

        /**
         * @brief Apply current low-pass gain value to a bus DSP.
         * @param bus Mixer bus.
         */
        void _applyBusLowPassGain(Bus bus);

        /**
         * @brief Clamp low-pass gain into valid device-supported range.
         * @param gain Requested gain value.
         * @return Clamped gain value.
         */
        static float _clampLowPassGain(float gain);
        static float _clampLowPassResonance(float resonance);

        /**
         * @brief Convert normalized low-pass gain to cutoff frequency.
         * @param gain Normalized gain value.
         * @return Cutoff frequency in Hz.
         */
        static float _lowPassGainToCutoffHz(float gain);
    };

} // namespace Audio

extern Audio::FmodAudioDevice* gAudioDevice;

#endif
