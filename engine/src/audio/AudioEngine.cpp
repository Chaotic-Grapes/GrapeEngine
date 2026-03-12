/* Start Header *****************************************************************/
/*!
\file   AudioEngine.cpp
\author Dalton Koh
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

    // load a cue through the active device
    bool AudioEngine::LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params) {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->LoadCue(cueId, filePath, params);
    }

    // unload a cue through the active device
    void AudioEngine::UnloadCue(const std::string& cueId) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->UnloadCue(cueId);
    }

    // check if a cue exists on the active device
    bool AudioEngine::HasCue(const std::string& cueId) const {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->HasCue(cueId);
    }

    // play a cue and cache instance state
    PlaybackHandle AudioEngine::Play(const std::string& cueId, const PlaySettings& settings, Bus bus) {
        // return empty handle when no device exists
        if (!m_device) {
            return {};
        }

        // ask device to start playback
        PlaybackHandle handle = m_device->Play(cueId, settings, bus);
        if (!handle) {
            return {};
        }

        // store runtime state for later updates
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

    // play a cue with play policy and cache instance state
    PlaybackHandle AudioEngine::PlaySingle(const std::string& cueId, const PlaySettings& settings, PlayPolicy policy, Bus bus) {
        // return empty handle when no device exists
        if (!m_device) {
            return {};
        }

        // ask device to enforce single play policy
        PlaybackHandle handle = m_device->PlaySingle(cueId, settings, policy, bus);
        if (!handle) {
            return {};
        }

        // create or refresh cached instance state
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

    // stop one handle and remove cached state
    void AudioEngine::Stop(PlaybackHandle handle, StopMode mode) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        // stop device handle then erase cache entry
        m_device->Stop(handle, mode);
        m_instances.erase(handle.Id);
    }

    // stop one cue using cue level tracking
    void AudioEngine::StopCue(const std::string& cueId, StopMode mode) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->StopCue(cueId, mode);
    }

    // query cue playback from the device
    bool AudioEngine::IsCuePlaying(const std::string& cueId) const {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->IsCuePlaying(cueId);
    }

    // update cached base volume for an instance
    void AudioEngine::SetInstanceVolume(PlaybackHandle handle, float volume) {
        // write cached value when handle is tracked
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.BaseVolume = volume;
        }
    }

    // update pitch in cache and on device
    void AudioEngine::SetInstancePitch(PlaybackHandle handle, float pitch) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.Pitch = pitch;
        }
        // push pitch to device
        m_device->SetInstancePitch(handle, pitch);
    }

    // update pan in cache and on device for non spatial sources
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

    // set per instance low pass gain
    void AudioEngine::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetInstanceLowPassGain(handle, gain);
    }

    // set 3d attributes for an instance
    void AudioEngine::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetInstancePosition(handle, pos, vel);
    }

    // set listener attributes for spatial audio
    void AudioEngine::SetListener(const ListenerParams& listener) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetListener(listener);
    }

    // set bus volume and clear bus fade state
    void AudioEngine::SetBusVolume(Bus bus, float volume) {
        // convert enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return;
        }
        // clamp volume to valid range
        m_busStates[index].Volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
        m_busStates[index].Fade.Active = false;
    }

    // read current volume for a bus
    float AudioEngine::GetBusVolume(Bus bus) const {
        // convert enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return 1.0f;
        }
        return m_busStates[index].Volume;
    }

    // begin a volume fade on one bus
    void AudioEngine::FadeBusVolume(Bus bus, float targetVolume, float duration) {
        // convert enum to array index
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

    // set low pass gain on a bus
    void AudioEngine::SetBusLowPassGain(Bus bus, float gain) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetBusLowPassGain(bus, gain);
    }

    // read low pass gain for a bus
    float AudioEngine::GetBusLowPassGain(Bus bus) const {
        // return default when no device exists
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetBusLowPassGain(bus);
    }

    // begin a fade for one instance
    void AudioEngine::FadeInstance(PlaybackHandle handle, float targetVolume, float duration, bool stopOnComplete) {
        // find tracked state for handle
        auto it = m_instances.find(handle.Id);
        if (it == m_instances.end()) {
            return;
        }
        InstanceState& state = it->second;
        // use current fade value so chained fades stay smooth
        state.Fade.Active = true;
        state.Fade.FromVolume = _computePreMixVolume(state);
        state.Fade.ToVolume = targetVolume;
        state.Fade.Duration = duration;
        state.Fade.Elapsed = 0.0f;
        state.Fade.StopOnComplete = stopOnComplete;
    }

    // fade all active instances to zero
    void AudioEngine::FadeOutAll(float duration) {
        // fade every active instance to zero then stop
        for (auto& [id, state] : m_instances) {
            state.Fade.Active = true;
            state.Fade.FromVolume = _computePreMixVolume(state);
            state.Fade.ToVolume = 0.0f;
            state.Fade.Duration = duration;
            state.Fade.Elapsed = 0.0f;
            state.Fade.StopOnComplete = true;
        }
    }

    // fade only instances on one bus to zero
    void AudioEngine::FadeOutBus(Bus bus, float duration) {
        // scan all tracked instances and filter by bus
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

    // report if any stop on complete fade is active
    bool AudioEngine::HasActiveFadeOuts() const {
        // look for active terminal fades
        for (const auto& [id, state] : m_instances) {
            if (state.Fade.Active && state.Fade.StopOnComplete && state.Fade.ToVolume <= 0.0f) {
                return true;
            }
        }
        return false;
    }

    // return max remaining time among terminal fades
    float AudioEngine::GetMaxFadeOutRemaining() const {
        // track largest remaining fade time
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

    // check if one handle is currently fading
    bool AudioEngine::IsHandleFading(PlaybackHandle handle) const {
        // find tracked state for handle
        auto it = m_instances.find(handle.Id);
        if (it == m_instances.end()) {
            return false;
        }
        return it->second.Fade.Active;
    }

    // check if one handle is tracked as active
    bool AudioEngine::IsHandleActive(PlaybackHandle handle) const {
        // active means handle exists in map
        return m_instances.find(handle.Id) != m_instances.end();
    }

    // stop all active fades immediately
    void AudioEngine::StopAllFades() {
        // ignore when no device exists
        if (!m_device) {
            return;
        }

        // stop all fading instances and remove them
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

    // update fade systems and prune ended handles
    void AudioEngine::Update(float deltaTime) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }

        _updateBusFades(deltaTime);
        _updateInstanceFades(deltaTime);
        _pruneStoppedInstances();
    }

    // compute instance volume before bus and master mix
    float AudioEngine::_computePreMixVolume(const InstanceState& instance) const {
        if (instance.Fade.Active) {
            // compute local fade volume before bus and master mix
            float t = instance.Fade.Duration > 0.0f ? (instance.Fade.Elapsed / instance.Fade.Duration) : 1.0f;
            if (t > 1.0f) {
                t = 1.0f;
            }
            return instance.Fade.FromVolume + (instance.Fade.ToVolume - instance.Fade.FromVolume) * t;
        }
        return instance.BaseVolume;
    }

    // compute combined bus multiplier for an instance
    float AudioEngine::_computeBusVolume(const InstanceState& instance) const {
        // master and bus multiply together
        float master = GetBusVolume(Bus::Master);
        float bus = GetBusVolume(instance.BusType);
        return master * bus;
    }

    // advance all active bus fades
    void AudioEngine::_updateBusFades(float deltaTime) {
        for (size_t i = 0; i < static_cast<size_t>(Bus::Count); ++i) {
            auto& state = m_busStates[i];
            if (!state.Fade.Active) {
                continue;
            }

            // update bus fade value
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

    // advance all instance fades and apply final volume
    void AudioEngine::_updateInstanceFades(float deltaTime) {
        // collect ids to stop after the loop
        std::vector<uint64_t> toStop;
        for (auto& [id, state] : m_instances) {
            if (!state.Fade.Active) {
                // apply bus and master mix even when no fade is active
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
                    // queue stop and remove after loop
                    toStop.push_back(id);
                }
                else {
                    // keep fade end value as the new base volume
                    state.BaseVolume = state.Fade.ToVolume;
                }
            }
        }

        for (uint64_t id : toStop) {
            if (auto it = m_instances.find(id); it != m_instances.end()) {
                // stop then erase the finished handle
                m_device->Stop(it->second.Handle, StopMode::Immediate);
                m_instances.erase(it);
            }
        }
    }

    // remove cached instances that finished on the device
    void AudioEngine::_pruneStoppedInstances() {
        // remove handles that are no longer playing
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
