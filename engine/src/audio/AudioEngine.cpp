/* Start Header *****************************************************************/
/*!
\file   AudioEngine.cpp
\author Dalton Koh 2403250
\par    d.koh@digipen.edu
\brief
Implements the audio engine runtime layer that tracks active playback handles
and applies runtime mixing controls.

Responsibilities
- route play and stop calls to the active audio device
- track per instance runtime state for volume pan pitch and bus
- apply bus volume and per instance volume fades over time
- expose per instance and bus low pass controls
- prune stopped handles and keep runtime state in sync

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
/* End Header *******************************************************************/

#include "audio/AudioEngine.h"

namespace Audio {
    AudioEngine* gAudioEngine = nullptr;

    // load one cue on the active device
    bool AudioEngine::LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params) {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->LoadCue(cueId, filePath, params);
    }

    // unload one cue on the active device
    void AudioEngine::UnloadCue(const std::string& cueId) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->UnloadCue(cueId);
    }

    // check whether a cue exists
    bool AudioEngine::HasCue(const std::string& cueId) const {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->HasCue(cueId);
    }

    // play a cue and cache runtime state
    PlaybackHandle AudioEngine::Play(const std::string& cueId, const PlaySettings& settings, Bus bus) {
        // return empty handle when no device exists
        if (!m_device) {
            return {};
        }

        // start playback on the device
        PlaybackHandle handle = m_device->Play(cueId, settings, bus);
        if (!handle) {
            return {};
        }

        // cache playback state for runtime updates
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

    // play cue with single instance policy
    PlaybackHandle AudioEngine::PlaySingle(const std::string& cueId, const PlaySettings& settings, PlayPolicy policy, Bus bus) {
        // return empty handle when no device exists
        if (!m_device) {
            return {};
        }

        // start playback with policy on the device
        PlaybackHandle handle = m_device->PlaySingle(cueId, settings, policy, bus);
        if (!handle) {
            return {};
        }

        // create or refresh cached state
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

    // stop one handle and clear cached state
    void AudioEngine::Stop(PlaybackHandle handle, StopMode mode) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        // stop device handle then erase cache entry
        m_device->Stop(handle, mode);
        m_instances.erase(handle.Id);
    }

    // stop an active cue mapping
    void AudioEngine::StopCue(const std::string& cueId, StopMode mode) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->StopCue(cueId, mode);
    }

    // query cue playback from device
    bool AudioEngine::IsCuePlaying(const std::string& cueId) const {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->IsCuePlaying(cueId);
    }

    // set cached base volume for a handle
    void AudioEngine::SetInstanceVolume(PlaybackHandle handle, float volume) {
        // update cache only when handle exists
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.BaseVolume = volume;
        }
    }

    // set pitch in cache and on device
    void AudioEngine::SetInstancePitch(PlaybackHandle handle, float pitch) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.Pitch = pitch;
        }
        // apply pitch to device channel
        m_device->SetInstancePitch(handle, pitch);
    }

    // set pan in cache and on device for 2d sources
    void AudioEngine::SetInstancePan(PlaybackHandle handle, float pan) {
        // ignore when no device exists
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

    // set per instance low pass gain on device
    void AudioEngine::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetInstanceLowPassGain(handle, gain);
    }

    // set 3d position and velocity for one handle
    void AudioEngine::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetInstancePosition(handle, pos, vel);
    }

    // set current listener data on device
    void AudioEngine::SetListener(const ListenerParams& listener) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetListener(listener);
    }

    // set bus volume and clear bus fade
    void AudioEngine::SetBusVolume(Bus bus, float volume) {
        // convert enum to bus array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return;
        }
        // clamp volume to valid range
        m_busStates[index].Volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
        m_busStates[index].Fade.Active = false;
    }

    // read current bus volume
    float AudioEngine::GetBusVolume(Bus bus) const {
        // convert enum to bus array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return 1.0f;
        }
        return m_busStates[index].Volume;
    }

    // start a volume fade on one bus
    void AudioEngine::FadeBusVolume(Bus bus, float targetVolume, float duration) {
        // convert enum to bus array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return;
        }

        // configure fade values for this bus
        BusState& state = m_busStates[index];
        state.Fade.Active = true;
        state.Fade.FromVolume = state.Volume;
        state.Fade.ToVolume = targetVolume;
        state.Fade.Duration = duration;
        state.Fade.Elapsed = 0.0f;
        state.Fade.StopOnComplete = false;
    }

    // set low pass gain on one bus
    void AudioEngine::SetBusLowPassGain(Bus bus, float gain) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetBusLowPassGain(bus, gain);
    }

    // get low pass gain on one bus
    float AudioEngine::GetBusLowPassGain(Bus bus) const {
        // return default when no device exists
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetBusLowPassGain(bus);
    }

    // start fade for one instance
    void AudioEngine::FadeInstance(PlaybackHandle handle, float targetVolume, float duration, bool stopOnComplete) {
        // find cached state for handle
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

    // fade all active instances to zero
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

    // fade all instances on a bus to zero
    void AudioEngine::FadeOutBus(Bus bus, float duration) {
        // iterate all instances and filter by bus
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

    // check for active terminal fade outs
    bool AudioEngine::HasActiveFadeOuts() const {
        // find any fade that will stop on complete
        for (const auto& [id, state] : m_instances) {
            if (state.Fade.Active && state.Fade.StopOnComplete && state.Fade.ToVolume <= 0.0f) {
                return true;
            }
        }
        return false;
    }

    // return max remaining time across terminal fades
    float AudioEngine::GetMaxFadeOutRemaining() const {
        // track highest remaining fade time
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

    // check if a handle is currently fading
    bool AudioEngine::IsHandleFading(PlaybackHandle handle) const {
        // resolve handle in cache
        auto it = m_instances.find(handle.Id);
        if (it == m_instances.end()) {
            return false;
        }
        return it->second.Fade.Active;
    }

    // check if a handle is tracked as active
    bool AudioEngine::IsHandleActive(PlaybackHandle handle) const {
        // active means cached handle exists
        return m_instances.find(handle.Id) != m_instances.end();
    }

    // stop all active fades immediately
    void AudioEngine::StopAllFades() {
        // ignore when no device exists
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

    // run one update tick for audio runtime state
    void AudioEngine::Update(float deltaTime) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }

        // update bus and instance fades then cleanup
        _updateBusFades(deltaTime);
        _updateInstanceFades(deltaTime);
        _pruneStoppedInstances();
    }

    // compute instance volume before bus and master mix
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

    // compute combined master and bus volume multiplier
    float AudioEngine::_computeBusVolume(const InstanceState& instance) const {
        // multiply master and bus gains
        float master = GetBusVolume(Bus::Master);
        float bus = GetBusVolume(instance.BusType);
        return master * bus;
    }

    // advance all active bus fades
    void AudioEngine::_updateBusFades(float deltaTime) {
        // walk all buses and update active fades
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

    // advance instance fades and apply final runtime volume
    void AudioEngine::_updateInstanceFades(float deltaTime) {
        // collect ids that should stop after loop
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
                // stop handle then remove cached entry
                m_device->Stop(it->second.Handle, StopMode::Immediate);
                m_instances.erase(it);
            }
        }
    }

    // remove handles that are no longer playing
    void AudioEngine::_pruneStoppedInstances() {
        // collect dead handles first
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
