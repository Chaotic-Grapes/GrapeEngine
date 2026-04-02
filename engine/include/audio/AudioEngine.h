/* Start Header *****************************************************************/
/*!
\file   AudioEngine.h
\author Dalton Koh
\par    d.koh@digipen.edu
\brief
Declares the audio engine runtime layer used for playback control and mixing.

Description
- tracks active playback handles and runtime instance state
- routes play and stop calls to the active audio device
- applies bus and instance volume fades over time
- exposes bus and instance low pass controls
- provides helper queries for fade and handle state

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "audio/FmodAudioDevice.h"
#include "audio/SoundTypes.h"
#include <unordered_map>
#include <vector>

namespace Audio {
    // runtime audio engine facade used by systems and services
    class AudioEngine {
    public:
        // default constructor
        AudioEngine() = default;

        // construct with an initial device
        explicit AudioEngine(FmodAudioDevice* device) : m_device(device) {}

        // set current device pointer
        void SetDevice(FmodAudioDevice* device) {
            // store device pointer for runtime calls
            m_device = device;
        }

        // get current device pointer
        FmodAudioDevice* Device() const {
            // return stored device pointer
            return m_device;
        }

        // load one cue into device storage
        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);

        // unload one cue from device storage
        void UnloadCue(const std::string& cueId);

        // check if one cue is loaded
        bool HasCue(const std::string& cueId) const;

        // play one cue and return playback handle
        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings, Bus bus);

        // play one cue with single instance policy
        PlaybackHandle PlaySingle(const std::string& cueId, const PlaySettings& settings, PlayPolicy policy, Bus bus);

        // stop one playback handle
        void Stop(PlaybackHandle handle, StopMode mode);

        // stop one cue level playback mapping
        void StopCue(const std::string& cueId, StopMode mode);

        // query cue playback state
        bool IsCuePlaying(const std::string& cueId) const;

        // set cached base volume for one handle
        void SetInstanceVolume(PlaybackHandle handle, float volume);

        // set pitch for one handle
        void SetInstancePitch(PlaybackHandle handle, float pitch);

        // set pan for one handle
        void SetInstancePan(PlaybackHandle handle, float pan);

        // set low pass gain for one handle
        void SetInstanceLowPassGain(PlaybackHandle handle, float gain);

        // set 3d transform for one handle
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel);
        void SetInstance3DMinMaxDistance(PlaybackHandle handle, float minDistance, float maxDistance);
        float GetInstance3DMinDistance(PlaybackHandle handle) const;
        float GetInstance3DMaxDistance(PlaybackHandle handle) const;
        void SetInstance3DSpread(PlaybackHandle handle, float spread);
        float GetInstance3DSpread(PlaybackHandle handle) const;
        void SetInstance3DLevel(PlaybackHandle handle, float level);
        float GetInstance3DLevel(PlaybackHandle handle) const;

        // Global defaults for Spatial3D playback.
        void SetDefault3DMinMaxDistance(float minDistance, float maxDistance);
        float GetDefault3DMinDistance() const;
        float GetDefault3DMaxDistance() const;
        void SetDefault3DSpread(float spread);
        float GetDefault3DSpread() const;
        void SetDefault3DLevel(float level);
        float GetDefault3DLevel() const;

        // set listener transform on device
        void SetListener(const ListenerParams& listener);

        // set one bus volume
        void SetBusVolume(Bus bus, float volume);

        // get one bus volume
        float GetBusVolume(Bus bus) const;

        // fade one bus volume over time
        void FadeBusVolume(Bus bus, float targetVolume, float duration);

        // set low pass gain on one bus
        void SetBusLowPassGain(Bus bus, float gain);

        // get low pass gain on one bus
        float GetBusLowPassGain(Bus bus) const;

        // set low pass resonance (Q) on one bus
        void SetBusLowPassResonance(Bus bus, float resonance);

        // get low pass resonance (Q) on one bus
        float GetBusLowPassResonance(Bus bus) const;

        // fade one instance toward target volume
        void FadeInstance(PlaybackHandle handle, float targetVolume, float duration, bool stopOnComplete);

        // fade all active instances to zero
        void FadeOutAll(float duration);

        // fade all active instances on one bus
        void FadeOutBus(Bus bus, float duration);

        // check if any terminal fade out is active
        bool HasActiveFadeOuts() const;

        // return maximum remaining fade out time
        float GetMaxFadeOutRemaining() const;

        // check if one handle is fading
        bool IsHandleFading(PlaybackHandle handle) const;

        // check if one handle is tracked as active
        bool IsHandleActive(PlaybackHandle handle) const;

        // stop all active fades immediately
        void StopAllFades();

        // update fades and cleanup once per frame
        void Update(float deltaTime);

    private:
        // per fade runtime state
        struct FadeState {
            bool Active = false;
            float FromVolume = 0.0f;
            float ToVolume = 0.0f;
            float Duration = 0.0f;
            float Elapsed = 0.0f;
            bool StopOnComplete = false;
        };

        // per instance runtime state
        struct InstanceState {
            PlaybackHandle Handle{};
            Bus BusType = Bus::SFX;
            float BaseVolume = 1.0f;
            float Pitch = 1.0f;
            float Pan = 0.0f;
            bool Spatial3D = false;
            FadeState Fade{};
        };

        // per bus runtime state
        struct BusState {
            float Volume = 1.0f;
            FadeState Fade{};
        };

        // compute instance volume before bus and master mix
        float _computePreMixVolume(const InstanceState& instance) const;

        // compute combined bus and master multiplier
        float _computeBusVolume(const InstanceState& instance) const;

        // advance active bus fades
        void _updateBusFades(float deltaTime);

        // advance active instance fades
        void _updateInstanceFades(float deltaTime);

        // remove stopped handles from cache
        void _pruneStoppedInstances();

        // active device pointer used for runtime calls
        FmodAudioDevice* m_device = nullptr;

        // map from handle id to instance state
        std::unordered_map<uint64_t, InstanceState> m_instances;

        // bus state array indexed by bus enum
        BusState m_busStates[static_cast<size_t>(Bus::Count)]{};
    };

    // global engine pointer used by runtime systems
    extern AudioEngine* gAudioEngine;
}

#endif
