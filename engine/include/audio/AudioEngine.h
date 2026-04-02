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
        /**
         * @brief Construct an audio engine with no attached device.
         */
        AudioEngine() = default;

        /**
         * @brief Construct an audio engine with an initial device.
         * @param device Initial FMOD device pointer.
         */
        explicit AudioEngine(FmodAudioDevice* device) : m_device(device) {}

        /**
         * @brief Set the active device used for all runtime audio calls.
         * @param device Device pointer, or nullptr to detach.
         */
        void SetDevice(FmodAudioDevice* device) {
            // store device pointer for runtime calls
            m_device = device;
        }

        /**
         * @brief Get the currently attached device pointer.
         * @return Active device pointer, or nullptr if detached.
         */
        FmodAudioDevice* Device() const {
            // return stored device pointer
            return m_device;
        }

        /**
         * @brief Load one cue into device storage.
         * @param cueId Logical cue identifier.
         * @param filePath Source audio file path.
         * @param params Cue loading parameters.
         * @return True when cue load succeeds.
         */
        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);

        /**
         * @brief Unload one cue from device storage.
         * @param cueId Logical cue identifier.
         */
        void UnloadCue(const std::string& cueId);

        /**
         * @brief Check if a cue id is currently loaded.
         * @param cueId Logical cue identifier.
         * @return True if the cue exists in device storage.
         */
        bool HasCue(const std::string& cueId) const;

        /**
         * @brief Play one cue and return a playback handle.
         * @param cueId Logical cue identifier.
         * @param settings Per-play invocation settings.
         * @param bus Mixer bus route.
         * @return Playback handle for runtime control.
         */
        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings, Bus bus);

        /**
         * @brief Play one cue with single-instance behavior policy.
         * @param cueId Logical cue identifier.
         * @param settings Per-play invocation settings.
         * @param policy Single-instance policy to apply.
         * @param bus Mixer bus route.
         * @return Playback handle for runtime control.
         */
        PlaybackHandle PlaySingle(const std::string& cueId, const PlaySettings& settings, PlayPolicy policy, Bus bus);

        /**
         * @brief Stop one playback handle.
         * @param handle Playback handle to stop.
         * @param mode Stop behavior.
         */
        void Stop(PlaybackHandle handle, StopMode mode);

        /**
         * @brief Stop playback mapped to one cue id.
         * @param cueId Logical cue identifier.
         * @param mode Stop behavior.
         */
        void StopCue(const std::string& cueId, StopMode mode);

        /**
         * @brief Query whether a cue is currently playing.
         * @param cueId Logical cue identifier.
         * @return True if cue playback is active.
         */
        bool IsCuePlaying(const std::string& cueId) const;

        /**
         * @brief Set cached base volume for one handle.
         * @param handle Playback handle.
         * @param volume Linear gain value.
         */
        void SetInstanceVolume(PlaybackHandle handle, float volume);

        /**
         * @brief Set pitch for one handle.
         * @param handle Playback handle.
         * @param pitch Pitch multiplier.
         */
        void SetInstancePitch(PlaybackHandle handle, float pitch);

        /**
         * @brief Set stereo pan for one handle.
         * @param handle Playback handle.
         * @param pan Pan value in range [-1, 1].
         */
        void SetInstancePan(PlaybackHandle handle, float pan);

        /**
         * @brief Set low-pass gain for one handle.
         * @param handle Playback handle.
         * @param gain Low-pass gain factor.
         */
        void SetInstanceLowPassGain(PlaybackHandle handle, float gain);

        /**
         * @brief Set 3D transform for one handle.
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

        // Global defaults for Spatial3D playback.
        void SetDefault3DMinMaxDistance(float minDistance, float maxDistance);
        float GetDefault3DMinDistance() const;
        float GetDefault3DMaxDistance() const;
        void SetDefault3DSpread(float spread);
        float GetDefault3DSpread() const;
        void SetDefault3DLevel(float level);
        float GetDefault3DLevel() const;

        /**
         * @brief Set listener transform on the active device.
         * @param listener Listener position/orientation payload.
         */
        void SetListener(const ListenerParams& listener);

        /**
         * @brief Set mixer bus volume.
         * @param bus Mixer bus.
         * @param volume Linear gain value.
         */
        void SetBusVolume(Bus bus, float volume);

        /**
         * @brief Get mixer bus volume.
         * @param bus Mixer bus.
         * @return Current linear gain value.
         */
        float GetBusVolume(Bus bus) const;

        /**
         * @brief Fade one bus volume over time.
         * @param bus Mixer bus.
         * @param targetVolume Destination gain.
         * @param duration Fade duration in seconds.
         */
        void FadeBusVolume(Bus bus, float targetVolume, float duration);

        /**
         * @brief Set low-pass gain on one bus.
         * @param bus Mixer bus.
         * @param gain Low-pass gain factor.
         */
        void SetBusLowPassGain(Bus bus, float gain);

        /**
         * @brief Get low-pass gain on one bus.
         * @param bus Mixer bus.
         * @return Low-pass gain factor.
         */
        float GetBusLowPassGain(Bus bus) const;

        // set low pass resonance (Q) on one bus
        void SetBusLowPassResonance(Bus bus, float resonance);

        // get low pass resonance (Q) on one bus
        float GetBusLowPassResonance(Bus bus) const;

        // fade one instance toward target volume
        /**
         * @brief Fade one instance toward a target volume.
         * @param handle Playback handle.
         * @param targetVolume Destination gain.
         * @param duration Fade duration in seconds.
         * @param stopOnComplete True to stop playback when fade completes.
         */
        void FadeInstance(PlaybackHandle handle, float targetVolume, float duration, bool stopOnComplete);

        /**
         * @brief Fade all active instances toward silence.
         * @param duration Fade duration in seconds.
         */
        void FadeOutAll(float duration);

        /**
         * @brief Fade all active instances on one bus toward silence.
         * @param bus Mixer bus.
         * @param duration Fade duration in seconds.
         */
        void FadeOutBus(Bus bus, float duration);

        /**
         * @brief Check whether any terminal fade-out is active.
         * @return True if one or more fade-outs are active.
         */
        bool HasActiveFadeOuts() const;

        /**
         * @brief Get the maximum remaining fade-out time.
         * @return Maximum remaining fade time in seconds.
         */
        float GetMaxFadeOutRemaining() const;

        /**
         * @brief Check if one playback handle is currently fading.
         * @param handle Playback handle.
         * @return True when the handle has an active fade.
         */
        bool IsHandleFading(PlaybackHandle handle) const;

        /**
         * @brief Check if one playback handle is tracked as active.
         * @param handle Playback handle.
         * @return True when the handle is tracked by runtime state.
         */
        bool IsHandleActive(PlaybackHandle handle) const;

        /**
         * @brief Cancel all active fades immediately.
         */
        void StopAllFades();

        /**
         * @brief Advance fades and prune stale runtime entries.
         * @param deltaTime Frame delta time in seconds.
         */
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

        /**
         * @brief Compute per-instance volume before bus/master multipliers.
         * @param instance Instance runtime state.
         * @return Premix linear gain value.
         */
        float _computePreMixVolume(const InstanceState& instance) const;

        /**
         * @brief Compute combined bus/master multiplier for one instance.
         * @param instance Instance runtime state.
         * @return Combined linear multiplier.
         */
        float _computeBusVolume(const InstanceState& instance) const;

        /**
         * @brief Advance active bus fades.
         * @param deltaTime Frame delta time in seconds.
         */
        void _updateBusFades(float deltaTime);

        /**
         * @brief Advance active instance fades.
         * @param deltaTime Frame delta time in seconds.
         */
        void _updateInstanceFades(float deltaTime);

        /**
         * @brief Remove stopped handles from cached runtime state.
         */
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
