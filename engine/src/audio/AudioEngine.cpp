/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file   AudioEngine.cpp
 *
 * @brief  High-level audio runtime for mixing, fades, and policy.
 */

#include "audio/AudioEngine.h"

namespace Audio {
    AudioEngine* gAudioEngine = nullptr;

    bool AudioEngine::LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params) {
        if (!m_device) {
            return false;
        }
        return m_device->LoadCue(cueId, filePath, params);
    }

    void AudioEngine::UnloadCue(const std::string& cueId) {
        if (!m_device) {
            return;
        }
        m_device->UnloadCue(cueId);
    }

    bool AudioEngine::HasCue(const std::string& cueId) const {
        if (!m_device) {
            return false;
        }
        return m_device->HasCue(cueId);
    }

    PlaybackHandle AudioEngine::Play(const std::string& cueId, const PlaySettings& settings, Bus bus) {
        if (!m_device) {
            return {};
        }

        PlaybackHandle handle = m_device->Play(cueId, settings, bus);
        if (!handle) {
            return {};
        }

        InstanceState state{};
        state.Handle = handle;
        state.BusType = bus;
        state.BaseVolume = settings.Volume;
        state.Pitch = settings.Pitch;
        state.Pan = settings.Pan;
        state.Spatial3D = settings.Spatial3D;
        state.Fade = {};
        m_instances[handle.Id] = state;
        return handle;
    }

    PlaybackHandle AudioEngine::PlaySingle(const std::string& cueId, const PlaySettings& settings, PlayPolicy policy, Bus bus) {
        if (!m_device) {
            return {};
        }

        PlaybackHandle handle = m_device->PlaySingle(cueId, settings, policy, bus);
        if (!handle) {
            return {};
        }

        auto& state = m_instances[handle.Id];
        state.Handle = handle;
        state.BusType = bus;
        state.BaseVolume = settings.Volume;
        state.Pitch = settings.Pitch;
        state.Pan = settings.Pan;
        state.Spatial3D = settings.Spatial3D;
        state.Fade = {};
        return handle;
    }

    void AudioEngine::Stop(PlaybackHandle handle, StopMode mode) {
        if (!m_device) {
            return;
        }
        m_device->Stop(handle, mode);
        m_instances.erase(handle.Id);
    }

    void AudioEngine::StopCue(const std::string& cueId, StopMode mode) {
        if (!m_device) {
            return;
        }
        m_device->StopCue(cueId, mode);
    }

    bool AudioEngine::IsCuePlaying(const std::string& cueId) const {
        if (!m_device) {
            return false;
        }
        return m_device->IsCuePlaying(cueId);
    }

    void AudioEngine::SetInstanceVolume(PlaybackHandle handle, float volume) {
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.BaseVolume = volume;
        }
    }

    void AudioEngine::SetInstancePitch(PlaybackHandle handle, float pitch) {
        if (!m_device) {
            return;
        }
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.Pitch = pitch;
        }
        m_device->SetInstancePitch(handle, pitch);
    }

    void AudioEngine::SetInstancePan(PlaybackHandle handle, float pan) {
        if (!m_device) {
            return;
        }
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.Pan = pan;
            if (!it->second.Spatial3D) {
                m_device->SetInstancePan(handle, pan);
            }
        }
    }

    void AudioEngine::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        if (!m_device) {
            return;
        }
        m_device->SetInstanceLowPassGain(handle, gain);
    }

    void AudioEngine::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        if (!m_device) {
            return;
        }
        m_device->SetInstancePosition(handle, pos, vel);
    }

    void AudioEngine::SetListener(const ListenerParams& listener) {
        if (!m_device) {
            return;
        }
        m_device->SetListener(listener);
    }

    void AudioEngine::SetBusVolume(Bus bus, float volume) {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return;
        }
        m_busStates[index].Volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
        m_busStates[index].Fade.Active = false;
    }

    float AudioEngine::GetBusVolume(Bus bus) const {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return 1.0f;
        }
        return m_busStates[index].Volume;
    }

    void AudioEngine::FadeBusVolume(Bus bus, float targetVolume, float duration) {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return;
        }

        BusState& state = m_busStates[index];
        state.Fade.Active = true;
        state.Fade.FromVolume = state.Volume;
        state.Fade.ToVolume = targetVolume;
        state.Fade.Duration = duration;
        state.Fade.Elapsed = 0.0f;
        state.Fade.StopOnComplete = false;
    }

    void AudioEngine::SetBusLowPassGain(Bus bus, float gain) {
        if (!m_device) {
            return;
        }
        m_device->SetBusLowPassGain(bus, gain);
    }

    float AudioEngine::GetBusLowPassGain(Bus bus) const {
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetBusLowPassGain(bus);
    }

    void AudioEngine::FadeInstance(PlaybackHandle handle, float targetVolume, float duration, bool stopOnComplete) {
        auto it = m_instances.find(handle.Id);
        if (it == m_instances.end()) {
            return;
        }
        InstanceState& state = it->second;
        // Start from the currently interpolated value so chaining fades is smooth.
        state.Fade.Active = true;
        state.Fade.FromVolume = _computePreMixVolume(state);
        state.Fade.ToVolume = targetVolume;
        state.Fade.Duration = duration;
        state.Fade.Elapsed = 0.0f;
        state.Fade.StopOnComplete = stopOnComplete;
    }

    void AudioEngine::FadeOutAll(float duration) {
        // Mark all active instances for terminal fade -> stop.
        for (auto& [id, state] : m_instances) {
            state.Fade.Active = true;
            state.Fade.FromVolume = _computePreMixVolume(state);
            state.Fade.ToVolume = 0.0f;
            state.Fade.Duration = duration;
            state.Fade.Elapsed = 0.0f;
            state.Fade.StopOnComplete = true;
        }
    }

    void AudioEngine::FadeOutBus(Bus bus, float duration) {
        for (auto& [id, state] : m_instances) {
            if (state.BusType != bus) {
                continue;
            }
            state.Fade.Active = true;
            state.Fade.FromVolume = _computePreMixVolume(state);
            state.Fade.ToVolume = 0.0f;
            state.Fade.Duration = duration;
            state.Fade.Elapsed = 0.0f;
            state.Fade.StopOnComplete = true;
        }
    }

    bool AudioEngine::HasActiveFadeOuts() const {
        for (const auto& [id, state] : m_instances) {
            if (state.Fade.Active && state.Fade.StopOnComplete && state.Fade.ToVolume <= 0.0f) {
                return true;
            }
        }
        return false;
    }

    float AudioEngine::GetMaxFadeOutRemaining() const {
        float maxRemaining = 0.0f;
        for (const auto& [id, state] : m_instances) {
            if (state.Fade.Active && state.Fade.StopOnComplete && state.Fade.ToVolume <= 0.0f) {
                float remaining = state.Fade.Duration - state.Fade.Elapsed;
                if (remaining > maxRemaining) {
                    maxRemaining = remaining;
                }
            }
        }
        return maxRemaining;
    }

    bool AudioEngine::IsHandleFading(PlaybackHandle handle) const {
        auto it = m_instances.find(handle.Id);
        if (it == m_instances.end()) {
            return false;
        }
        return it->second.Fade.Active;
    }

    bool AudioEngine::IsHandleActive(PlaybackHandle handle) const {
        return m_instances.find(handle.Id) != m_instances.end();
    }

    void AudioEngine::StopAllFades() {
        if (!m_device) {
            return;
        }

        // Stop all instances that are currently fading and remove them
        std::vector<uint64_t> toRemove;
        for (auto& [id, state] : m_instances) {
            if (state.Fade.Active) {
                m_device->Stop(state.Handle, StopMode::Immediate);
                toRemove.push_back(id);
            }
        }

        for (uint64_t id : toRemove) {
            m_instances.erase(id);
        }
    }

    void AudioEngine::Update(float deltaTime) {
        if (!m_device) {
            return;
        }

        _updateBusFades(deltaTime);
        _updateInstanceFades(deltaTime);
        _pruneStoppedInstances();
    }

    float AudioEngine::_computePreMixVolume(const InstanceState& instance) const {
        if (instance.Fade.Active) {
            // Fade interpolation runs in instance-local volume space before bus/master gain.
            float t = instance.Fade.Duration > 0.0f ? (instance.Fade.Elapsed / instance.Fade.Duration) : 1.0f;
            if (t > 1.0f) {
                t = 1.0f;
            }
            return instance.Fade.FromVolume + (instance.Fade.ToVolume - instance.Fade.FromVolume) * t;
        }
        return instance.BaseVolume;
    }

    float AudioEngine::_computeBusVolume(const InstanceState& instance) const {
        float master = GetBusVolume(Bus::Master);
        float bus = GetBusVolume(instance.BusType);
        return master * bus;
    }

    void AudioEngine::_updateBusFades(float deltaTime) {
        for (size_t i = 0; i < static_cast<size_t>(Bus::Count); ++i) {
            auto& state = m_busStates[i];
            if (!state.Fade.Active) {
                continue;
            }

            // Bus fades are independent from per-instance fades and multiply later.
            state.Fade.Elapsed += deltaTime;
            float t = state.Fade.Duration > 0.0f ? (state.Fade.Elapsed / state.Fade.Duration) : 1.0f;
            if (t >= 1.0f) {
                state.Volume = state.Fade.ToVolume;
                state.Fade.Active = false;
                continue;
            }
            state.Volume = state.Fade.FromVolume + (state.Fade.ToVolume - state.Fade.FromVolume) * t;
        }
    }

    void AudioEngine::_updateInstanceFades(float deltaTime) {
        std::vector<uint64_t> toStop;
        for (auto& [id, state] : m_instances) {
            if (!state.Fade.Active) {
                // Even without an active fade, final gain is still affected by bus/master mix.
                float finalVolume = _computePreMixVolume(state) * _computeBusVolume(state);
                m_device->SetInstanceVolume(state.Handle, finalVolume);
                if (!state.Spatial3D) {
                    m_device->SetInstancePan(state.Handle, state.Pan);
                }
                continue;
            }

            state.Fade.Elapsed += deltaTime;
            float t = state.Fade.Duration > 0.0f ? (state.Fade.Elapsed / state.Fade.Duration) : 1.0f;
            if (t > 1.0f) {
                t = 1.0f;
            }
            float fadeVolume = state.Fade.FromVolume + (state.Fade.ToVolume - state.Fade.FromVolume) * t;
            float finalVolume = fadeVolume * _computeBusVolume(state);
            m_device->SetInstanceVolume(state.Handle, finalVolume);

            if (!state.Spatial3D) {
                m_device->SetInstancePan(state.Handle, state.Pan);
            }

            if (t >= 1.0f) {
                state.Fade.Active = false;
                if (state.Fade.StopOnComplete) {
                    // Defer stop/removal until after iteration to avoid iterator invalidation.
                    toStop.push_back(id);
                }
                else {
                    // Non-terminal fades commit their end value as the new steady base volume.
                    state.BaseVolume = state.Fade.ToVolume;
                }
            }
        }

        for (uint64_t id : toStop) {
            if (auto it = m_instances.find(id); it != m_instances.end()) {
                m_device->Stop(it->second.Handle, StopMode::Immediate);
                m_instances.erase(it);
            }
        }
    }

    void AudioEngine::_pruneStoppedInstances() {
        std::vector<uint64_t> toRemove;
        for (auto& [id, state] : m_instances) {
            if (!m_device->IsHandlePlaying(state.Handle)) {
                toRemove.push_back(id);
            }
        }

        for (uint64_t id : toRemove) {
            m_instances.erase(id);
        }
    }
}
