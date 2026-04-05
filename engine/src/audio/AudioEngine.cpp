/* Start Header *****************************************************************/
/*!
\file   AudioEngine.cpp
\author Dalton Koh (100%)
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

    /**
     * @brief Loads a named audio cue on the active device from the given file path.
     * @param cueId    String identifier used to reference this cue at runtime.
     * @param filePath Filesystem path to the audio asset.
     * @param params   Sound creation parameters such as streaming and 3D flags.
     * @return True if the cue was loaded successfully, false if the device is unavailable or loading failed.
     */
    bool AudioEngine::LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params) {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->LoadCue(cueId, filePath, params);
    }

    /**
     * @brief Unloads a previously loaded audio cue from the active device, freeing its resources.
     * @param cueId String identifier of the cue to unload.
     */
    void AudioEngine::UnloadCue(const std::string& cueId) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->UnloadCue(cueId);
    }

    /**
     * @brief Queries whether a cue with the given id is currently loaded on the device.
     * @param cueId String identifier of the cue to check.
     * @return True if the cue is loaded and available for playback.
     */
    bool AudioEngine::HasCue(const std::string& cueId) const {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->HasCue(cueId);
    }

    /**
     * @brief Starts playback of a cue and caches the resulting instance state for runtime mixing.
     * @param cueId    String identifier of the cue to play.
     * @param settings Playback parameters including volume, pitch, pan, looping, and spatial flags.
     * @param bus      Mixing bus to route this instance through.
     * @return A valid PlaybackHandle identifying the new instance, or an invalid handle on failure.
     */
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

    /**
     * @brief Starts or manages playback of a cue under a single-instance policy.
     * @param cueId    String identifier of the cue to play.
     * @param settings Playback parameters including volume, pitch, pan, looping, and spatial flags.
     * @param policy   Rule that governs behavior when the cue is already playing (restart, resume, ignore, or new instance).
     * @param bus      Mixing bus to route this instance through.
     * @return A valid PlaybackHandle for the active instance, or an invalid handle on failure.
     */
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

    /**
     * @brief Stops a specific playback instance and removes it from the runtime state cache.
     * @param handle Handle of the instance to stop.
     * @param mode   Whether to stop immediately or fade out first.
     */
    void AudioEngine::Stop(PlaybackHandle handle, StopMode mode) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        // stop device handle then erase cache entry
        m_device->Stop(handle, mode);
        m_instances.erase(handle.Id);
    }

    /**
     * @brief Stops the single-instance playback currently mapped to a cue identifier.
     * @param cueId String identifier of the cue whose active instance should be stopped.
     * @param mode  Whether to stop immediately or fade out first.
     */
    void AudioEngine::StopCue(const std::string& cueId, StopMode mode) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->StopCue(cueId, mode);
    }

    /**
     * @brief Queries the device to determine whether any instance of a cue is currently playing.
     * @param cueId String identifier of the cue to query.
     * @return True if the cue is actively playing on the device.
     */
    bool AudioEngine::IsCuePlaying(const std::string& cueId) const {
        // return false when no device exists
        if (!m_device) {
            return false;
        }
        return m_device->IsCuePlaying(cueId);
    }

    /**
     * @brief Updates the cached base volume for a playback instance; the final volume is applied during the next Update tick.
     * @param handle Handle of the instance to modify.
     * @param volume New base volume in the range [0, 1].
     */
    void AudioEngine::SetInstanceVolume(PlaybackHandle handle, float volume) {
        // update cache only when handle exists
        if (auto it = m_instances.find(handle.Id); it != m_instances.end()) {
            it->second.BaseVolume = volume;
        }
    }

    /**
     * @brief Updates the pitch of a playback instance in the cache and immediately on the device.
     * @param handle Handle of the instance to modify.
     * @param pitch  New pitch multiplier (1.0 = normal speed).
     */
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

    /**
     * @brief Updates the stereo pan of a 2D playback instance in the cache and immediately on the device.
     * @param handle Handle of the instance to modify.
     * @param pan    Pan value in the range [-1, 1] where -1 is full left and 1 is full right.
     */
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

    /**
     * @brief Sets the per-instance low-pass filter gain directly on the device channel.
     * @param handle Handle of the instance to modify.
     * @param gain   Low-pass gain in the range [0, 1] where 1 is fully open (no filtering).
     */
    void AudioEngine::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetInstanceLowPassGain(handle, gain);
    }

    /**
     * @brief Updates the 3D world-space position and velocity of a playback instance on the device.
     * @param handle Handle of the instance to move.
     * @param pos    World-space position of the audio emitter.
     * @param vel    World-space velocity of the audio emitter used for Doppler calculation.
     */
    void AudioEngine::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetInstancePosition(handle, pos, vel);
    }

    /**
     * @brief Sets the minimum and maximum 3D rolloff distances for a specific playback instance.
     * @param handle      Handle of the instance to modify.
     * @param minDistance Distance at which the sound reaches full volume.
     * @param maxDistance Distance beyond which the sound is inaudible.
     */
    void AudioEngine::SetInstance3DMinMaxDistance(PlaybackHandle handle, float minDistance, float maxDistance) {
        if (!m_device) {
            return;
        }
        m_device->SetInstance3DMinMaxDistance(handle, minDistance, maxDistance);
    }

    /**
     * @brief Returns the minimum 3D rolloff distance currently set for a playback instance.
     * @param handle Handle of the instance to query.
     * @return Minimum distance value, or 1.0 if the device is unavailable.
     */
    float AudioEngine::GetInstance3DMinDistance(PlaybackHandle handle) const {
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetInstance3DMinDistance(handle);
    }

    /**
     * @brief Returns the maximum 3D rolloff distance currently set for a playback instance.
     * @param handle Handle of the instance to query.
     * @return Maximum distance value, or 25.0 if the device is unavailable.
     */
    float AudioEngine::GetInstance3DMaxDistance(PlaybackHandle handle) const {
        if (!m_device) {
            return 25.0f;
        }
        return m_device->GetInstance3DMaxDistance(handle);
    }

    /**
     * @brief Sets the 3D speaker spread angle for a specific playback instance.
     * @param handle Handle of the instance to modify.
     * @param spread Spread angle in degrees [0, 360]; 0 collapses to mono point source.
     */
    void AudioEngine::SetInstance3DSpread(PlaybackHandle handle, float spread) {
        if (!m_device) {
            return;
        }
        m_device->SetInstance3DSpread(handle, spread);
    }

    /**
     * @brief Returns the 3D speaker spread angle currently set for a playback instance.
     * @param handle Handle of the instance to query.
     * @return Spread angle in degrees, or 0.0 if the device is unavailable.
     */
    float AudioEngine::GetInstance3DSpread(PlaybackHandle handle) const {
        if (!m_device) {
            return 0.0f;
        }
        return m_device->GetInstance3DSpread(handle);
    }

    /**
     * @brief Sets the 3D spatialization blend level for a specific playback instance.
     * @param handle Handle of the instance to modify.
     * @param level  Blend factor in [0, 1]; 0 is fully 2D panned, 1 is fully 3D spatialized.
     */
    void AudioEngine::SetInstance3DLevel(PlaybackHandle handle, float level) {
        if (!m_device) {
            return;
        }
        m_device->SetInstance3DLevel(handle, level);
    }

    /**
     * @brief Returns the 3D spatialization blend level currently set for a playback instance.
     * @param handle Handle of the instance to query.
     * @return 3D level blend factor, or 1.0 if the device is unavailable.
     */
    float AudioEngine::GetInstance3DLevel(PlaybackHandle handle) const {
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetInstance3DLevel(handle);
    }

    /**
     * @brief Sets the default minimum and maximum 3D rolloff distances applied to newly spawned instances.
     * @param minDistance Default distance at which sounds reach full volume.
     * @param maxDistance Default distance beyond which sounds become inaudible.
     */
    void AudioEngine::SetDefault3DMinMaxDistance(float minDistance, float maxDistance) {
        if (!m_device) {
            return;
        }
        m_device->SetDefault3DMinMaxDistance(minDistance, maxDistance);
    }

    /**
     * @brief Returns the default minimum 3D rolloff distance used for new playback instances.
     * @return Default minimum distance, or 1.0 if the device is unavailable.
     */
    float AudioEngine::GetDefault3DMinDistance() const {
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetDefault3DMinDistance();
    }

    /**
     * @brief Returns the default maximum 3D rolloff distance used for new playback instances.
     * @return Default maximum distance, or 25.0 if the device is unavailable.
     */
    float AudioEngine::GetDefault3DMaxDistance() const {
        if (!m_device) {
            return 25.0f;
        }
        return m_device->GetDefault3DMaxDistance();
    }

    /**
     * @brief Sets the default 3D speaker spread angle applied to newly spawned instances.
     * @param spread Default spread angle in degrees [0, 360].
     */
    void AudioEngine::SetDefault3DSpread(float spread) {
        if (!m_device) {
            return;
        }
        m_device->SetDefault3DSpread(spread);
    }

    /**
     * @brief Returns the default 3D speaker spread angle used for new playback instances.
     * @return Default spread angle in degrees, or 0.0 if the device is unavailable.
     */
    float AudioEngine::GetDefault3DSpread() const {
        if (!m_device) {
            return 0.0f;
        }
        return m_device->GetDefault3DSpread();
    }

    /**
     * @brief Sets the default 3D spatialization blend level applied to newly spawned instances.
     * @param level Default blend factor in [0, 1].
     */
    void AudioEngine::SetDefault3DLevel(float level) {
        if (!m_device) {
            return;
        }
        m_device->SetDefault3DLevel(level);
    }

    /**
     * @brief Returns the default 3D spatialization blend level used for new playback instances.
     * @return Default 3D level factor, or 1.0 if the device is unavailable.
     */
    float AudioEngine::GetDefault3DLevel() const {
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetDefault3DLevel();
    }

    /**
     * @brief Forwards listener position, velocity, and orientation to the active audio device for 3D spatialization.
     * @param listener Struct containing the listener's world-space position, velocity, forward, and up vectors.
     */
    void AudioEngine::SetListener(const ListenerParams& listener) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetListener(listener);
    }

    /**
     * @brief Sets the volume of a mixing bus immediately, cancelling any active fade on that bus.
     * @param bus    Target bus to adjust.
     * @param volume New volume in [0, 1]; values outside this range are clamped.
     */
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

    /**
     * @brief Returns the current volume of a mixing bus.
     * @param bus Target bus to query.
     * @return Current bus volume in [0, 1], or 1.0 if the bus index is out of range.
     */
    float AudioEngine::GetBusVolume(Bus bus) const {
        // convert enum to bus array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(Bus::Count)) {
            return 1.0f;
        }
        return m_busStates[index].Volume;
    }

    /**
     * @brief Initiates a linear volume fade on a mixing bus from its current volume to a target.
     * @param bus          Target bus to fade.
     * @param targetVolume Destination volume at the end of the fade.
     * @param duration     Duration of the fade in seconds.
     */
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

    /**
     * @brief Sets the low-pass filter gain for a mixing bus DSP node.
     * @param bus  Target bus to filter.
     * @param gain Low-pass gain in [0, 1] where 1 is fully open and 0 is maximum filtering.
     */
    void AudioEngine::SetBusLowPassGain(Bus bus, float gain) {
        // ignore when no device exists
        if (!m_device) {
            return;
        }
        m_device->SetBusLowPassGain(bus, gain);
    }

    /**
     * @brief Returns the current low-pass filter gain for a mixing bus.
     * @param bus Target bus to query.
     * @return Current low-pass gain, or 1.0 if the device is unavailable.
     */
    float AudioEngine::GetBusLowPassGain(Bus bus) const {
        // return default when no device exists
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetBusLowPassGain(bus);
    }

    /**
     * @brief Sets the resonance (Q factor) of the low-pass filter DSP node on a mixing bus.
     * @param bus       Target bus to modify.
     * @param resonance Resonance value; higher values produce a more pronounced peak at the cutoff frequency.
     */
    void AudioEngine::SetBusLowPassResonance(Bus bus, float resonance) {
        if (!m_device) {
            return;
        }
        m_device->SetBusLowPassResonance(bus, resonance);
    }

    /**
     * @brief Returns the current low-pass filter resonance value for a mixing bus.
     * @param bus Target bus to query.
     * @return Current resonance value, or 1.0 if the device is unavailable.
     */
    float AudioEngine::GetBusLowPassResonance(Bus bus) const {
        if (!m_device) {
            return 1.0f;
        }
        return m_device->GetBusLowPassResonance(bus);
    }

    /**
     * @brief Initiates a volume fade on a single playback instance, optionally stopping it when the fade completes.
     * @param handle         Handle of the instance to fade.
     * @param targetVolume   Destination volume at the end of the fade.
     * @param duration       Duration of the fade in seconds.
     * @param stopOnComplete If true, the instance is automatically stopped once the fade reaches the target.
     */
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

    /**
     * @brief Schedules a terminal fade-to-zero on every active playback instance, stopping each when done.
     * @param duration Duration of the fade-out in seconds.
     */
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

    /**
     * @brief Schedules a terminal fade-to-zero on all playback instances routed through a specific bus.
     * @param bus      Target bus whose instances should be faded out and stopped.
     * @param duration Duration of the fade-out in seconds.
     */
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

    /**
     * @brief Returns whether any tracked instance currently has an active terminal (stop-on-complete) fade-out.
     * @return True if at least one instance is fading out to silence and will be stopped on completion.
     */
    bool AudioEngine::HasActiveFadeOuts() const {
        // find any fade that will stop on complete
        for (const auto& [id, state] : m_instances) {
            if (state.Fade.Active && state.Fade.StopOnComplete && state.Fade.ToVolume <= 0.0f) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Returns the longest remaining time among all active terminal fade-out operations.
     * @return Maximum remaining fade duration in seconds, or 0.0 if no terminal fades are active.
     */
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

    /**
     * @brief Returns whether a specific playback instance currently has an active volume fade in progress.
     * @param handle Handle of the instance to query.
     * @return True if the instance exists in the cache and its fade flag is active.
     */
    bool AudioEngine::IsHandleFading(PlaybackHandle handle) const {
        // resolve handle in cache
        auto it = m_instances.find(handle.Id);
        if (it == m_instances.end()) {
            return false;
        }
        return it->second.Fade.Active;
    }

    /**
     * @brief Returns whether a playback handle is still tracked as an active instance in the engine cache.
     * @param handle Handle to check.
     * @return True if the handle exists in the instance map.
     */
    bool AudioEngine::IsHandleActive(PlaybackHandle handle) const {
        // active means cached handle exists
        return m_instances.find(handle.Id) != m_instances.end();
    }

    /**
     * @brief Immediately stops and removes all playback instances that currently have an active fade.
     */
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

    /**
     * @brief Advances all bus fades, instance fades, and runtime volume application for one frame tick.
     * @param deltaTime Elapsed time since the last update in seconds.
     */
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

    /**
     * @brief Computes the pre-mix volume for an instance, interpolating through any active fade.
     * @param instance Const reference to the instance state to evaluate.
     * @return Interpolated volume in instance-local space before bus and master gain are applied.
     */
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

    /**
     * @brief Computes the combined master and per-bus volume multiplier for an instance.
     * @param instance Const reference to the instance state used to identify the bus.
     * @return Product of the master bus volume and the instance's assigned bus volume.
     */
    float AudioEngine::_computeBusVolume(const InstanceState& instance) const {
        // multiply master and bus gains
        float master = GetBusVolume(Bus::Master);
        float bus = GetBusVolume(instance.BusType);
        return master * bus;
    }

    /**
     * @brief Advances the fade timer for every active bus fade and updates bus volume accordingly.
     * @param deltaTime Elapsed time since the last update in seconds.
     */
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

    /**
     * @brief Advances instance fade timers, applies final mixed volumes to device channels, and
     *        stops instances whose terminal fades have completed.
     * @param deltaTime Elapsed time since the last update in seconds.
     */
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

    /**
     * @brief Removes all instance cache entries whose underlying device channels have stopped playing.
     */
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
