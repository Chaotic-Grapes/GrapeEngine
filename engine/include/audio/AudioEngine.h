/*
* @file AudioEngine.h
* @brief High-level audio runtime for mixing, fades, and policy.
*/

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "audio/FmodAudioDevice.h"
#include "audio/SoundTypes.h"
#include <unordered_map>
#include <vector>

namespace Audio {
    class AudioEngine {
    public:
        AudioEngine() = default;
        explicit AudioEngine(FmodAudioDevice* device) : m_device(device) {}

        void SetDevice(FmodAudioDevice* device) { m_device = device; }
        FmodAudioDevice* Device() const { return m_device; }

        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);
        void UnloadCue(const std::string& cueId);
        bool HasCue(const std::string& cueId) const;

        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings, Bus bus);
        PlaybackHandle PlaySingle(const std::string& cueId, const PlaySettings& settings, PlayPolicy policy, Bus bus);
        void Stop(PlaybackHandle handle, StopMode mode);
        void StopCue(const std::string& cueId, StopMode mode);
        bool IsCuePlaying(const std::string& cueId) const;

        void SetInstanceVolume(PlaybackHandle handle, float volume);
        void SetInstancePitch(PlaybackHandle handle, float pitch);
        void SetInstancePan(PlaybackHandle handle, float pan);
        void SetInstanceLowPassGain(PlaybackHandle handle, float gain);
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel);
        void SetListener(const ListenerParams& listener);

        void SetBusVolume(Bus bus, float volume);
        float GetBusVolume(Bus bus) const;
        void FadeBusVolume(Bus bus, float targetVolume, float duration);

        void FadeInstance(PlaybackHandle handle, float targetVolume, float duration, bool stopOnComplete);
        void FadeOutAll(float duration);
        void FadeOutBus(Bus bus, float duration);

        bool HasActiveFadeOuts() const;
        float GetMaxFadeOutRemaining() const;
        bool IsHandleFading(PlaybackHandle handle) const;
        bool IsHandleActive(PlaybackHandle handle) const;

        void StopAllFades();
        void Update(float deltaTime);

    private:
        struct FadeState {
            bool Active = false;
            float FromVolume = 0.0f;
            float ToVolume = 0.0f;
            float Duration = 0.0f;
            float Elapsed = 0.0f;
            bool StopOnComplete = false;
        };

        struct InstanceState {
            PlaybackHandle Handle{};
            Bus BusType = Bus::SFX;
            float BaseVolume = 1.0f;
            float Pitch = 1.0f;
            float Pan = 0.0f;
            bool Spatial3D = false;
            FadeState Fade{};
        };

        struct BusState {
            float Volume = 1.0f;
            FadeState Fade{};
        };

        float _computePreMixVolume(const InstanceState& instance) const;
        float _computeBusVolume(const InstanceState& instance) const;
        void _updateBusFades(float deltaTime);
        void _updateInstanceFades(float deltaTime);
        void _pruneStoppedInstances();

        FmodAudioDevice* m_device = nullptr;
        std::unordered_map<uint64_t, InstanceState> m_instances;
        BusState m_busStates[static_cast<size_t>(Bus::Count)]{};
    };

    extern AudioEngine* gAudioEngine;
}

#endif
