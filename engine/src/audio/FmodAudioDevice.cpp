/* Start Header *****************************************************************/
/*!
\file   FmodAudioDevice.cpp
\author Dalton Koh (100%)
\par    d.koh@digipen.edu
\brief
Implements the FMOD audio device backend used by the engine runtime.

Description
- owns fmod system startup and shutdown
- loads cues from memory or file paths
- plays and stops instances and single instance cues
- updates listener and per instance runtime parameters
- manages bus routing and bus low pass dsp nodes

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "audio/FmodAudioDevice.h"
#include "core/Logger.h"
#include "services/ResourceManager.h"
#include <cmath>
#include <fmod_dsp_effects.h>

namespace {
    // keep low pass cutoff range for bus filters
    constexpr float kMinBusLowPassCutoffHz = 100.0f;
    constexpr float kMaxBusLowPassCutoffHz = 22000.0f;
    constexpr float kMinBusLowPassResonance = 1.0f;
    constexpr float kMaxBusLowPassResonance = 10.0f;

    // The game layer often treats X/Y as the "world plane" and Z as "up"
    // (out of the screen). FMOD uses Y as up. Convert engine vectors into
    // FMOD space by swapping Y/Z while preserving X so left/right panning
    // stays intuitive (world +X -> right ear).
    /**
     * @brief Converts an engine Vec3 into an FMOD_VECTOR, swapping Y and Z to match FMOD's coordinate system.
     * @param v Engine-space vector where X is right, Y is screen-horizontal, and Z is up.
     * @return FMOD_VECTOR with axes remapped for correct spatial audio positioning.
     */
    inline FMOD_VECTOR ToFmodVec(const Audio::Vec3& v) {
        return FMOD_VECTOR{ v.x, v.z, v.y };
    }

    /**
     * @brief Normalizes an FMOD_VECTOR, returning a fallback direction when the input is near-zero length.
     * @param v        Vector to normalize.
     * @param fallback Vector returned when the squared length of v is too small to normalize safely.
     * @return Normalized version of v, or fallback if v is effectively zero.
     */
    inline FMOD_VECTOR NormalizeOrDefault(FMOD_VECTOR v, FMOD_VECTOR fallback) {
        const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (len2 <= 1e-8f) {
            return fallback;
        }
        const float invLen = 1.0f / std::sqrt(len2);
        v.x *= invLen;
        v.y *= invLen;
        v.z *= invLen;
        return v;
    }

    /**
     * @brief Computes the dot product of two FMOD_VECTORs.
     * @param a First vector.
     * @param b Second vector.
     * @return Scalar dot product of a and b.
     */
    inline float Dot(FMOD_VECTOR a, FMOD_VECTOR b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    /**
     * @brief Checks an FMOD_RESULT and logs an error with optional context when the result is not FMOD_OK.
     * @param r   FMOD result code to evaluate.
     * @param ctx Optional string describing the operation that produced the result.
     * @return True if r equals FMOD_OK, false otherwise.
     */
    bool FMOD_OK_OR_LOG(const FMOD_RESULT r, const char* ctx = nullptr) {
        // return true on success
        if (r == FMOD_OK) return true;
        LOG_ERROR(std::string("FMOD error") + (ctx 
            ? (std::string(" (") + ctx + ")")
            : "") + ": " + std::to_string(r));
        return false;
    }

    /**
     * @brief Queries whether an FMOD channel is currently producing audio output.
     * @param ch Pointer to the FMOD channel to check; may be nullptr.
     * @return True if ch is non-null and reports an active playing state.
     */
    inline bool _is_playing(FMOD::Channel* ch) {
        // null channels are not playing
        if (!ch) return false;
        bool playing = false;
        if (ch->isPlaying(&playing) != FMOD_OK) return false;
        return playing;
    }

    /**
     * @brief Returns the FMOD channel mode flags appropriate for 2D or 3D spatialization.
     * @param spatial3D True to select 3D linear-rolloff mode; false selects 2D mode.
     * @return Composed FMOD_MODE value suitable for use with playSound or setMode.
     */
    inline FMOD_MODE SpatialChannelMode(bool spatial3D) {
        if (!spatial3D) {
            return FMOD_2D;
        }
        // Linear rolloff gives predictable attenuation from min -> max distance.
        return static_cast<FMOD_MODE>(FMOD_3D | FMOD_3D_LINEARROLLOFF);
    }
}

namespace Audio {

    /**
     * @brief Creates the FMOD system, initializes it on the default output device, and sets up bus routing.
     * @return True if FMOD was initialized successfully and bus routing is ready; false on any failure.
     */
    bool FmodAudioDevice::Initialize() {
        // reset bus objects before startup
        m_busGroups.fill(nullptr);
        m_busLowPassDsps.fill(nullptr);

        // Create system and initialize.
        if (!FMOD_OK_OR_LOG(FMOD::System_Create(&m_system), "System_Create"))
            return false;
        constexpr unsigned int kInitFlags = FMOD_INIT_NORMAL | FMOD_INIT_CHANNEL_LOWPASS;
        if (!FMOD_OK_OR_LOG(m_system->init(512, kInitFlags, nullptr), "init"))
            return false;
        // Grab master channel group.
        if (!FMOD_OK_OR_LOG(m_system->getMasterChannelGroup(&m_master), "getMasterChannelGroup")) {
            if (m_system) { m_system->release(); m_system = nullptr; }
            return false;
        }
        if (!_initializeBusRouting()) {
            _shutdownBusRouting();
            if (m_system) { m_system->close(); m_system->release(); m_system = nullptr; }
            m_master = nullptr;
            return false;
        }
        // Apply cached master volume.
        SetMasterVolume(m_masterVolume);
        return true;
    }

    /**
     * @brief Creates the FMOD system and initializes it on a specific output device identified by index or display name.
     * @param deviceID Numeric index string or display name of the desired output device.
     * @return True if FMOD was initialized on the requested device and bus routing is ready; false on any failure.
     */
    bool FmodAudioDevice::InitializeWithDevice(const std::string& deviceID) {
        // reset bus objects before startup
        m_busGroups.fill(nullptr);
        m_busLowPassDsps.fill(nullptr);

        // Create FMOD system first.
        if (!FMOD_OK_OR_LOG(FMOD::System_Create(&m_system), "System_Create"))
            return false;

        // Device IDs can arrive either as numeric index or display name depending
        // on where selection originated (settings UI, bootstrap config, etc).
        int driverIndex = 0;
        try {
            // parse numeric device id
            // Try to parse as integer first (0, 1, 2, etc.)
            driverIndex = std::stoi(deviceID);
        }
        catch (...) {
            // scan driver names when id is not numeric
            // If not a direct integer, search for device by name
            int numDrivers = 0;
            if (m_system->getNumDrivers(&numDrivers) == FMOD_OK) {
                for (int i = 0; i < numDrivers; ++i) {
                    char driverName[256] = { 0 };
                    if (m_system->getDriverInfo(i, driverName, sizeof(driverName), nullptr, nullptr, nullptr, nullptr) == FMOD_OK) {
                        if (deviceID == driverName) {
                            driverIndex = i;
                            break;
                        }
                    }
                }
            }
        }

        // Set the driver before initialization.
        if (!FMOD_OK_OR_LOG(m_system->setDriver(driverIndex), "setDriver"))
            return false;

        // Initialize FMOD with the selected driver.
        constexpr unsigned int kInitFlags = FMOD_INIT_NORMAL | FMOD_INIT_CHANNEL_LOWPASS;
        if (!FMOD_OK_OR_LOG(m_system->init(512, kInitFlags, nullptr), "init")) {
            if (m_system) { m_system->release(); m_system = nullptr; }
            return false;
        }

        // Grab master channel group.
        if (!FMOD_OK_OR_LOG(m_system->getMasterChannelGroup(&m_master), "getMasterChannelGroup")) {
            if (m_system) { m_system->release(); m_system = nullptr; }
            return false;
        }
        if (!_initializeBusRouting()) {
            _shutdownBusRouting();
            if (m_system) { m_system->close(); m_system->release(); m_system = nullptr; }
            m_master = nullptr;
            return false;
        }

        // Apply cached master volume.
        SetMasterVolume(m_masterVolume);
        return true;
    }

    /**
     * @brief Pumps the FMOD system update and evicts stale entries from the single-instance channel map.
     */
    void FmodAudioDevice::Update() {
        // Pump FMOD update.
        if (m_system) m_system->update();

        // Keep single-instance map clean so subsequent PlaySingle() policy decisions
        // are based on currently alive channels only.
        for (auto it = m_activeByCue.begin(); it != m_activeByCue.end(); ) {
            FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second });
            if (!ch || !_is_playing(ch)) it = m_activeByCue.erase(it);
            else ++it;
        }
    }

    /**
     * @brief Releases all FMOD channels, sounds, bus DSP nodes, and the FMOD system itself.
     */
    void FmodAudioDevice::Shutdown() {
        // Free channel user data.
        for (const auto& [id, ch] : m_channels) {
            void* data = nullptr;
            if (ch && ch->getUserData(&data) == FMOD_OK && data) {
                delete static_cast<uint64_t*>(data);
            }
        }
        m_channels.clear();
        m_activeByCue.clear();

        // Release sounds.
        for (auto& [cid, entry] : m_cues) {
            if (entry.Sound) { entry.Sound->release(); entry.Sound = nullptr; }
        }
        m_cues.clear();

        _shutdownBusRouting();

        // Shutdown FMOD.
        if (m_system) {
            m_system->close();
            m_system->release();
            m_system = nullptr;
        }
        m_master = nullptr;
    }

    /**
     * @brief Loads an audio cue, preferring in-memory loading via ResourceManager with a file-path fallback.
     * @param cueId    String identifier used to reference the cue after loading.
     * @param filePath Filesystem path or resource key for the audio asset.
     * @param params   Sound creation parameters such as streaming and 3D flags.
     * @return True if the cue is loaded (including if it was already loaded); false on failure.
     */
    bool FmodAudioDevice::LoadCue(const std::string& cueId,
        const std::string& filePath,
        const SoundParams& params)
    {
        // require an initialized fmod system
        if (!m_system) return false;
        if (m_cues.count(cueId)) return true; // already loaded

        // Prefer in-memory load via ResourceManager so packed/virtualized assets work.
        if (auto audioBytes = RM.Get<AudioData>(filePath)) {
            if (audioBytes->IsValid && !audioBytes->Data.empty()) {
                if (FMOD::Sound* s = _createSoundFromMemory(cueId, filePath, params)) {
                    m_cues.emplace(cueId, CueEntry{ s, params, filePath });
                    return true;
                }
                else {
                    LOG_WARNING("FMOD memory-load failed for " << filePath.c_str() << ", falling back to file path.");
                }
            }
        }

        // Fallback to direct file path when memory-backed load is unavailable.
        FMOD::Sound* s = nullptr;
        auto mode = params.Stream ? FMOD_CREATESTREAM : FMOD_DEFAULT;
        mode |= params.Is3D ? FMOD_3D : FMOD_2D;
        if (m_system->createSound(filePath.c_str(), mode, nullptr, &s) != FMOD_OK || !s)
            return false;

        m_cues.emplace(cueId, CueEntry{ s, params, filePath });
        return true;
    }

    /**
     * @brief Releases the FMOD sound object for a cue and removes it from the cue and single-instance maps.
     * @param cueId String identifier of the cue to unload.
     */
    void FmodAudioDevice::UnloadCue(const std::string& cueId) {
        // release cue sound if it exists
        if (const auto it = m_cues.find(cueId); it != m_cues.end()) {
            if (it->second.Sound) it->second.Sound->release();
            m_cues.erase(it);
        }
        m_activeByCue.erase(cueId);
    }

    /**
     * @brief Returns whether a cue with the given id has been loaded into the device.
     * @param cueId String identifier of the cue to check.
     * @return True if the cue entry exists in the internal cue map.
     */
    bool FmodAudioDevice::HasCue(const std::string& cueId) const {
        // lookup cue map
        return m_cues.count(cueId) > 0;
    }

    /**
     * @brief Starts playback of a loaded cue on a channel routed to the specified bus.
     * @param cueId String identifier of the cue to play; must have been loaded first.
     * @param s     Playback settings including volume, pitch, pan, loop, and spatial options.
     * @param bus   Mixing bus to route this channel through.
     * @return A valid PlaybackHandle for the new channel, or an invalid handle on failure.
     */
    PlaybackHandle FmodAudioDevice::Play(const std::string& cueId,
        const PlaySettings& s,
        Bus bus)
    {
        // find loaded cue data
        const auto it = m_cues.find(cueId);
        if (it == m_cues.end()) return {};

        FMOD::Sound* snd = it->second.Sound;
        if (!snd) return {};

        FMOD::ChannelGroup* targetGroup = _channelGroupForBus(bus);
        if (!targetGroup) {
            // fallback to master group
            targetGroup = m_master;
        }

        // Configure sound defaults for this play request.
        const FMOD_MODE playbackMode = static_cast<FMOD_MODE>(
            (s.Loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | SpatialChannelMode(s.Spatial3D));
        FMOD_OK_OR_LOG(snd->setMode(playbackMode), "Sound::setMode");
        FMOD_OK_OR_LOG(snd->setLoopCount(s.Loop ? -1 : 0), "Sound::setLoopCount");

        FMOD::Channel* ch = nullptr;
        if (!FMOD_OK_OR_LOG(m_system->playSound(snd, targetGroup, true, &ch), "playSound") || !ch)
            return {};

        // Apply channel settings.
        FMOD_OK_OR_LOG(ch->setMode(SpatialChannelMode(s.Spatial3D)), "Channel::setMode");
        if (s.Spatial3D) {
            // Default falloff tuned for small scenes; adjust if needed.
            FMOD_OK_OR_LOG(ch->set3DMinMaxDistance(m_default3DMinDistance, m_default3DMaxDistance), "Channel::set3DMinMaxDistance");
            FMOD_OK_OR_LOG(ch->set3DSpread(m_default3DSpread), "Channel::set3DSpread");
            FMOD_OK_OR_LOG(ch->set3DLevel(m_default3DLevel), "Channel::set3DLevel");
        }
        FMOD_OK_OR_LOG(ch->setVolume(s.Volume), "Channel::setVolume");
        FMOD_OK_OR_LOG(ch->setPitch(s.Pitch), "Channel::setPitch");
        if (!s.Spatial3D) {
            FMOD_OK_OR_LOG(ch->setPan(s.Pan), "Channel::setPan");
        }
        FMOD_OK_OR_LOG(ch->setPaused(s.StartPaused), "Channel::setPaused");

        // store channel by generated handle id
        PlaybackHandle h{ m_nextId++ };
        m_channels.emplace(h.Id, ch);
        auto* storedId = new uint64_t(h.Id);
        ch->setUserData(storedId);
        return h;
    }

    /**
     * @brief Stops the channel associated with a playback handle according to the specified stop mode.
     * @param handle Handle identifying the channel to stop.
     * @param mode   StopMode::Immediate halts playback at once; other modes zero volume first.
     */
    void FmodAudioDevice::Stop(PlaybackHandle handle, StopMode mode) {
        // resolve channel from handle
        if (auto* ch = _channelFromHandle(handle)) {
            if (mode == StopMode::Immediate) ch->stop();
            else { ch->setVolume(0.0f); ch->stop(); }
        }
    }

    /**
     * @brief Starts or manages a single tracked playback instance for a cue, applying the specified policy.
     * @param cueId  String identifier of the cue to play.
     * @param s      Playback settings including volume, pitch, pan, loop, and spatial options.
     * @param policy Rule governing behavior when an active instance already exists.
     * @param bus    Mixing bus to route the channel through.
     * @return A valid PlaybackHandle for the active or new instance, or an invalid handle on failure.
     */
    PlaybackHandle FmodAudioDevice::PlaySingle(const std::string& cueId,
        const PlaySettings& s,
        PlayPolicy policy,
        Bus bus)
    {
        // resolve routing group for requested bus
        FMOD::ChannelGroup* targetGroup = _channelGroupForBus(bus);
        if (!targetGroup) {
            targetGroup = m_master;
        }

        // reuse or replace active cue instance based on policy
        if (auto it = m_activeByCue.find(cueId); it != m_activeByCue.end()) {
            if (FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second })) {
                const bool playing = _is_playing(ch);
                // Policy decides whether to restart, resume, ignore, or spawn a new instance.
                switch (policy) {
                case PlayPolicy::SingleInstanceRestart:
                    FMOD_OK_OR_LOG(ch->setPosition(0, FMOD_TIMEUNIT_MS), "Channel::setPosition");
                    FMOD_OK_OR_LOG(ch->setChannelGroup(targetGroup), "Channel::setChannelGroup");
                    FMOD_OK_OR_LOG(ch->setMode(SpatialChannelMode(s.Spatial3D)), "Channel::setMode");
                    if (s.Spatial3D) {
                        // Default falloff tuned for small scenes; adjust if needed.
                        FMOD_OK_OR_LOG(ch->set3DMinMaxDistance(m_default3DMinDistance, m_default3DMaxDistance), "Channel::set3DMinMaxDistance");
                        FMOD_OK_OR_LOG(ch->set3DSpread(m_default3DSpread), "Channel::set3DSpread");
                        FMOD_OK_OR_LOG(ch->set3DLevel(m_default3DLevel), "Channel::set3DLevel");
                    }
                    if (!s.Spatial3D) {
                        FMOD_OK_OR_LOG(ch->setPan(s.Pan), "Channel::setPan");
                    }
                    FMOD_OK_OR_LOG(ch->setPaused(s.StartPaused ? true : false), "Channel::setPaused");
                    FMOD_OK_OR_LOG(ch->setVolume(s.Volume), "Channel::setVolume");
                    FMOD_OK_OR_LOG(ch->setPitch(s.Pitch), "Channel::setPitch");
                    return PlaybackHandle{ it->second };
                case PlayPolicy::SingleInstanceResume:
                    // Resume only affects paused/stopped state; we still reapply routing
                    // and current play settings so behavior stays consistent with fresh Play().
                    if (!playing && !s.StartPaused) FMOD_OK_OR_LOG(ch->setPaused(false), "Channel::setPaused");
                    if (s.StartPaused) FMOD_OK_OR_LOG(ch->setPaused(true), "Channel::setPaused");
                    FMOD_OK_OR_LOG(ch->setChannelGroup(targetGroup), "Channel::setChannelGroup");
                    FMOD_OK_OR_LOG(ch->setMode(SpatialChannelMode(s.Spatial3D)), "Channel::setMode");
                    if (s.Spatial3D) {
                        // Default falloff tuned for small scenes; adjust if needed.
                        FMOD_OK_OR_LOG(ch->set3DMinMaxDistance(m_default3DMinDistance, m_default3DMaxDistance), "Channel::set3DMinMaxDistance");
                        FMOD_OK_OR_LOG(ch->set3DSpread(m_default3DSpread), "Channel::set3DSpread");
                        FMOD_OK_OR_LOG(ch->set3DLevel(m_default3DLevel), "Channel::set3DLevel");
                    }
                    if (!s.Spatial3D) {
                        FMOD_OK_OR_LOG(ch->setPan(s.Pan), "Channel::setPan");
                    }
                    FMOD_OK_OR_LOG(ch->setVolume(s.Volume), "Channel::setVolume");
                    FMOD_OK_OR_LOG(ch->setPitch(s.Pitch), "Channel::setPitch");
                    return PlaybackHandle{ it->second };
                case PlayPolicy::SingleInstanceIgnore:
                    // Keep current playback untouched while active.
                    if (playing) return PlaybackHandle{ it->second };
                    break;
                case PlayPolicy::NewInstance:
                    // Fall through to normal Play() below.
                    break;
                }
            }
            else {
                // remove stale cue handle mapping
                m_activeByCue.erase(it);
            }
        }
        // create a new instance when policy allows
        auto h = Play(cueId, s, bus);
        if (h) m_activeByCue[cueId] = h.Id;
        return h;
    }

    /**
     * @brief Stops the single tracked instance for a cue and removes it from the active-cue map.
     * @param cueId String identifier of the cue whose active instance should be stopped.
     * @param mode  StopMode::Immediate halts playback at once; other modes zero volume first.
     */
    void FmodAudioDevice::StopCue(const std::string& cueId, StopMode mode) {
        // find active cue handle entry
        auto it = m_activeByCue.find(cueId);
        if (it == m_activeByCue.end()) return;
        if (FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second })) {
            if (mode == StopMode::Immediate) ch->stop();
            else { ch->setVolume(0.0f); ch->stop(); }
        }
        m_activeByCue.erase(it);
    }

    /**
     * @brief Returns whether the single tracked instance for a cue is currently producing audio.
     * @param cueId String identifier of the cue to query.
     * @return True if a tracked channel exists for the cue and reports an active playing state.
     */
    bool FmodAudioDevice::IsCuePlaying(const std::string& cueId) const {
        // find tracked cue handle
        auto it = m_activeByCue.find(cueId);
        if (it == m_activeByCue.end()) return false;
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(PlaybackHandle{ it->second }))
            return _is_playing(ch);
        return false;
    }

    /**
     * @brief Sets the volume of a channel identified by a playback handle directly on the FMOD channel.
     * @param handle Handle of the channel to modify.
     * @param volume New volume level; FMOD accepts values in [0, 1] for standard range.
     */
    void FmodAudioDevice::SetInstanceVolume(PlaybackHandle handle, float volume) {
        // write channel volume if handle resolves
        if (auto* ch = _channelFromHandle(handle)) ch->setVolume(volume);
    }

    /**
     * @brief Sets the pitch of a channel identified by a playback handle directly on the FMOD channel.
     * @param handle Handle of the channel to modify.
     * @param pitch  Pitch multiplier (1.0 = original pitch, 0.5 = one octave down, 2.0 = one octave up).
     */
    void FmodAudioDevice::SetInstancePitch(PlaybackHandle handle, float pitch) {
        // write channel pitch if handle resolves
        if (auto* ch = _channelFromHandle(handle)) ch->setPitch(pitch);
    }

    /**
     * @brief Sets the stereo pan of a channel, forcing 2D mode to ensure pan is applied correctly.
     * @param handle Handle of the channel to modify.
     * @param pan    Pan value in [-1, 1]; -1 is full left, 0 is center, 1 is full right.
     */
    void FmodAudioDevice::SetInstancePan(PlaybackHandle handle, float pan) {
        // write channel pan if handle resolves
        if (auto* ch = _channelFromHandle(handle)) {
            // Pan only applies reliably to 2D channels; force 2D mode here since some cues may have
            // been created with 3D defaults even when the gameplay layer wants camera-centered panning.
            ch->setMode(FMOD_2D);
            ch->setPan(pan);
        }
    }

    /**
     * @brief Sets the per-channel built-in low-pass gain on a playback instance's FMOD channel.
     * @param handle Handle of the channel to modify.
     * @param gain   Gain value in [0, 1]; clamped before being applied to the FMOD channel.
     */
    void FmodAudioDevice::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        // clamp and apply to channel low pass
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = _clampLowPassGain(gain);
            FMOD_OK_OR_LOG(ch->setLowPassGain(clamped), "Channel::setLowPassGain");
        }
    }

    /**
     * @brief Updates the 3D world-space position and velocity of a channel on the FMOD device.
     * @param handle Handle of the channel to reposition.
     * @param pos    World-space emitter position; converted to FMOD coordinate space internally.
     * @param vel    World-space emitter velocity used for Doppler shift calculation.
     */
    void FmodAudioDevice::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        // write position and velocity vectors
        if (auto* ch = _channelFromHandle(handle)) {
            const FMOD_VECTOR p = ToFmodVec(pos);
            const FMOD_VECTOR v = ToFmodVec(vel);
            ch->set3DAttributes(&p, &v);
        }
    }

    /**
     * @brief Sets the 3D rolloff min and max distances for a specific channel, clamping invalid values.
     * @param handle      Handle of the channel to modify.
     * @param minDistance Minimum audible distance (clamped to >= 0); full volume within this range.
     * @param maxDistance Maximum audible distance (clamped to >= minDistance); silent beyond this.
     */
    void FmodAudioDevice::SetInstance3DMinMaxDistance(PlaybackHandle handle, float minDistance, float maxDistance) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float minD = minDistance < 0.0f ? 0.0f : minDistance;
            const float maxD = maxDistance < minD ? minD : maxDistance;
            FMOD_OK_OR_LOG(ch->set3DMinMaxDistance(minD, maxD), "Channel::set3DMinMaxDistance");
        }
    }

    /**
     * @brief Reads the minimum 3D rolloff distance from a channel's current FMOD state.
     * @param handle Handle of the channel to query.
     * @return Current minimum distance, or the device default if the channel cannot be resolved.
     */
    float FmodAudioDevice::GetInstance3DMinDistance(PlaybackHandle handle) const {
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            float minD = m_default3DMinDistance;
            float maxD = m_default3DMaxDistance;
            if (FMOD_OK_OR_LOG(ch->get3DMinMaxDistance(&minD, &maxD), "Channel::get3DMinMaxDistance")) {
                return minD;
            }
        }
        return m_default3DMinDistance;
    }

    /**
     * @brief Reads the maximum 3D rolloff distance from a channel's current FMOD state.
     * @param handle Handle of the channel to query.
     * @return Current maximum distance, or the device default if the channel cannot be resolved.
     */
    float FmodAudioDevice::GetInstance3DMaxDistance(PlaybackHandle handle) const {
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            float minD = m_default3DMinDistance;
            float maxD = m_default3DMaxDistance;
            if (FMOD_OK_OR_LOG(ch->get3DMinMaxDistance(&minD, &maxD), "Channel::get3DMinMaxDistance")) {
                return maxD;
            }
        }
        return m_default3DMaxDistance;
    }

    /**
     * @brief Sets the 3D speaker spread angle for a channel, clamping to [0, 360] degrees.
     * @param handle Handle of the channel to modify.
     * @param spread Spread angle in degrees; 0 produces a mono point source, 360 spreads across all speakers.
     */
    void FmodAudioDevice::SetInstance3DSpread(PlaybackHandle handle, float spread) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = spread < 0.0f ? 0.0f : (spread > 360.0f ? 360.0f : spread);
            FMOD_OK_OR_LOG(ch->set3DSpread(clamped), "Channel::set3DSpread");
        }
    }

    /**
     * @brief Reads the 3D speaker spread angle from a channel's current FMOD state.
     * @param handle Handle of the channel to query.
     * @return Current spread angle in degrees, or the device default if the channel cannot be resolved.
     */
    float FmodAudioDevice::GetInstance3DSpread(PlaybackHandle handle) const {
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            float spread = m_default3DSpread;
            if (FMOD_OK_OR_LOG(ch->get3DSpread(&spread), "Channel::get3DSpread")) {
                return spread;
            }
        }
        return m_default3DSpread;
    }

    /**
     * @brief Sets the 3D spatialization blend level for a channel, clamping to [0, 1].
     * @param handle Handle of the channel to modify.
     * @param level  Blend factor; 0 is fully 2D panned, 1 is fully 3D spatialized.
     */
    void FmodAudioDevice::SetInstance3DLevel(PlaybackHandle handle, float level) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
            FMOD_OK_OR_LOG(ch->set3DLevel(clamped), "Channel::set3DLevel");
        }
    }

    /**
     * @brief Reads the 3D spatialization blend level from a channel's current FMOD state.
     * @param handle Handle of the channel to query.
     * @return Current 3D level blend factor, or the device default if the channel cannot be resolved.
     */
    float FmodAudioDevice::GetInstance3DLevel(PlaybackHandle handle) const {
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            float level = m_default3DLevel;
            if (FMOD_OK_OR_LOG(ch->get3DLevel(&level), "Channel::get3DLevel")) {
                return level;
            }
        }
        return m_default3DLevel;
    }

    /**
     * @brief Stores new default 3D rolloff distances used when spawning channels without explicit distance overrides.
     * @param minDistance Default minimum distance (clamped to >= 0).
     * @param maxDistance Default maximum distance (clamped to >= minDistance).
     */
    void FmodAudioDevice::SetDefault3DMinMaxDistance(float minDistance, float maxDistance) {
        const float minD = minDistance < 0.0f ? 0.0f : minDistance;
        const float maxD = maxDistance < minD ? minD : maxDistance;
        m_default3DMinDistance = minD;
        m_default3DMaxDistance = maxD;
    }

    /**
     * @brief Returns the stored default minimum 3D rolloff distance.
     * @return Default minimum distance value.
     */
    float FmodAudioDevice::GetDefault3DMinDistance() const {
        return m_default3DMinDistance;
    }

    /**
     * @brief Returns the stored default maximum 3D rolloff distance.
     * @return Default maximum distance value.
     */
    float FmodAudioDevice::GetDefault3DMaxDistance() const {
        return m_default3DMaxDistance;
    }

    /**
     * @brief Stores a new default 3D speaker spread angle applied to channels created without explicit spread values.
     * @param spread Spread angle in degrees, clamped to [0, 360].
     */
    void FmodAudioDevice::SetDefault3DSpread(float spread) {
        m_default3DSpread = spread < 0.0f ? 0.0f : (spread > 360.0f ? 360.0f : spread);
    }

    /**
     * @brief Returns the stored default 3D speaker spread angle.
     * @return Default spread angle in degrees.
     */
    float FmodAudioDevice::GetDefault3DSpread() const {
        return m_default3DSpread;
    }

    /**
     * @brief Stores a new default 3D spatialization blend level applied to channels created without an explicit level.
     * @param level Blend factor clamped to [0, 1].
     */
    void FmodAudioDevice::SetDefault3DLevel(float level) {
        m_default3DLevel = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    }

    /**
     * @brief Returns the stored default 3D spatialization blend level.
     * @return Default 3D level blend factor.
     */
    float FmodAudioDevice::GetDefault3DLevel() const {
        return m_default3DLevel;
    }

    /**
     * @brief Returns whether the channel for a given playback handle is currently producing audio.
     * @param handle Handle to look up and check.
     * @return True if the handle resolves to an active FMOD channel that reports a playing state.
     */
    bool FmodAudioDevice::IsHandlePlaying(PlaybackHandle handle) const {
        // resolve mutable access for helper call
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            return _is_playing(ch);
        }
        return false;
    }

    /**
     * @brief Sets the low-pass filter gain on a bus DSP node, clamping and applying the value immediately.
     * @param bus  Target bus whose low-pass DSP should be updated.
     * @param gain Gain value in [0, 1]; the DSP is bypassed when gain is near 1.0.
     */
    void FmodAudioDevice::SetBusLowPassGain(Bus bus, float gain) {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return;
        }

        m_busLowPassGain[index] = _clampLowPassGain(gain);
        _applyBusLowPassGain(bus);
    }

    /**
     * @brief Returns the currently stored low-pass filter gain for a bus.
     * @param bus Target bus to query.
     * @return Stored gain value in [0, 1], or 1.0 if the bus index is out of range.
     */
    float FmodAudioDevice::GetBusLowPassGain(Bus bus) const {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return 1.0f;
        }
        return m_busLowPassGain[index];
    }

    /**
     * @brief Sets the resonance (Q factor) on a bus low-pass DSP node, clamping and reapplying the filter.
     * @param bus       Target bus to modify.
     * @param resonance Resonance value; clamped to the supported FMOD range before application.
     */
    void FmodAudioDevice::SetBusLowPassResonance(Bus bus, float resonance) {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return;
        }

        m_busLowPassResonance[index] = _clampLowPassResonance(resonance);
        _applyBusLowPassGain(bus);
    }

    /**
     * @brief Returns the currently stored low-pass resonance value for a bus.
     * @param bus Target bus to query.
     * @return Stored resonance value, or 1.0 if the bus index is out of range.
     */
    float FmodAudioDevice::GetBusLowPassResonance(Bus bus) const {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return 1.0f;
        }
        return m_busLowPassResonance[index];
    }

    /**
     * @brief Updates the FMOD listener attributes for 3D audio spatialization.
     * @param l Listener parameters containing world-space position, velocity, forward, and up vectors.
     */
    void FmodAudioDevice::SetListener(const ListenerParams& l) {
        // require initialized fmod system
        if (!m_system) return;
        // Update listener attributes.
        const FMOD_VECTOR pos = ToFmodVec(l.Position);
        const FMOD_VECTOR vel = ToFmodVec(l.Velocity);

        // FMOD expects normalized, non-parallel orientation vectors.
        FMOD_VECTOR fwd = NormalizeOrDefault(ToFmodVec(l.Forward), FMOD_VECTOR{ 0.0f, 0.0f, 1.0f });
        FMOD_VECTOR up = NormalizeOrDefault(ToFmodVec(l.Up), FMOD_VECTOR{ 0.0f, 1.0f, 0.0f });
        if (std::fabs(Dot(fwd, up)) > 0.99f) {
            up = FMOD_VECTOR{ 0.0f, 1.0f, 0.0f };
        }
        m_system->set3DListenerAttributes(0, &pos, &vel, &fwd, &up);
    }

    /**
     * @brief Sets the master channel group volume, clamping to [0, 1] and caching the value.
     * @param volume New master volume; values outside [0, 1] are clamped before application.
     */
    void FmodAudioDevice::SetMasterVolume(float volume) {
        // clamp volume to valid range
        // Clamp and apply.
        m_masterVolume = (volume < 0.f) ? 0.f : (volume > 1.f ? 1.f : volume);
        if (m_master) m_master->setVolume(m_masterVolume);
    }

    /**
     * @brief Returns the cached master output volume.
     * @return Current master volume in [0, 1].
     */
    float FmodAudioDevice::GetMasterVolume() const {
        // return cached master value
        return m_masterVolume;
    }

    /**
     * @brief Pauses all audio output by pausing the FMOD master channel group.
     */
    void FmodAudioDevice::PauseAll() {
        // pause master channel group
        if (m_master) m_master->setPaused(true);
    }

    /**
     * @brief Resumes all audio output by unpausing the FMOD master channel group.
     */
    void FmodAudioDevice::ResumeAll() {
        // resume master channel group
        if (m_master) m_master->setPaused(false);
    }

    /**
     * @brief Fills a vector with (cueId, sourcePath) pairs for every currently loaded cue.
     * @param out Output vector that receives the cue id and source path for each loaded entry.
     */
    void FmodAudioDevice::GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const {
        // reset output container then fill
        // Return cue id -> path list.
        out.clear();
        out.reserve(m_cues.size());
        for (const auto& [id, entry] : m_cues)
            out.emplace_back(id, entry.SourcePath);
    }

    /**
     * @brief Returns an existing FMOD sound for a cue or creates and caches a new one from the file path.
     * @param cueId  String identifier for the sound entry in the cue map.
     * @param path   Filesystem path used to create the FMOD sound when not already cached.
     * @param params Sound creation parameters such as streaming and 3D flags.
     * @return Pointer to the FMOD sound, or nullptr if the system is unavailable or creation fails.
     */
    FMOD::Sound* FmodAudioDevice::_getOrCreateSound(const std::string& cueId,
        const std::string& path,
        const SoundParams& params)
    {
        // require initialized fmod system
        if (!m_system) return nullptr;
        if (const auto it = m_cues.find(cueId); it != m_cues.end())
            return it->second.Sound;

        // Create sound and cache.
        FMOD::Sound* s = nullptr;
        auto mode = params.Stream ? FMOD_CREATESTREAM : FMOD_DEFAULT;
        mode |= params.Is3D ? FMOD_3D : FMOD_2D;
        if (m_system->createSound(path.c_str(), mode, nullptr, &s) != FMOD_OK || !s)
            return nullptr;

        m_cues.emplace(cueId, CueEntry{ s, params, path });
        return s;
    }

    /**
     * @brief Resolves a PlaybackHandle to its corresponding FMOD channel pointer.
     * @param h Handle to look up in the channel map.
     * @return Pointer to the FMOD channel, or nullptr if the handle is invalid or not found.
     */
    FMOD::Channel* FmodAudioDevice::_channelFromHandle(PlaybackHandle h) {
        // reject invalid handles
        // Resolve channel by handle id.
        if (!h) return nullptr;
        const auto it = m_channels.find(h.Id);
        if (it == m_channels.end()) return nullptr;
        return it->second;
    }

    /**
     * @brief Creates an FMOD sound from raw bytes retrieved via ResourceManager, bypassing file I/O.
     * @param cueId  Unused; present for signature consistency with other sound creation helpers.
     * @param path   Resource key used to retrieve audio bytes from the ResourceManager.
     * @param params Sound creation parameters such as 3D flags.
     * @return Pointer to the newly created FMOD sound, or nullptr if data is unavailable or creation fails.
     */
    FMOD::Sound* FmodAudioDevice::_createSoundFromMemory(const std::string& cueId,
        const std::string& path,
        const SoundParams& params)
    {
        // cue id is unused in memory path loading
        // Ignore cue id; use the audio data by path.
        (void)cueId;

        auto audioBytes = RM.Get<AudioData>(path);
        if (!audioBytes || !audioBytes->IsValid || audioBytes->Data.empty())
            return nullptr;

        // Build FMOD open-from-memory info.
        FMOD_CREATESOUNDEXINFO exinfo{};
        exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        exinfo.length = static_cast<unsigned int>(audioBytes->Data.size());

        unsigned int mode = FMOD_OPENMEMORY | (params.Is3D ? FMOD_3D : FMOD_2D);

        FMOD::Sound* s = nullptr;
        const FMOD_RESULT r = m_system->createSound(
            reinterpret_cast<const char*>(audioBytes->Data.data()), mode, &exinfo, &s);
        if (r != FMOD_OK || !s) return nullptr;

        return s;
    }

    /**
     * @brief Creates an FMOD channel group and attaches a low-pass DSP node for each bus, parenting them under master.
     * @return True if all channel groups and DSP nodes were created and attached successfully; false on any failure.
     */
    bool FmodAudioDevice::_initializeBusRouting() {
        // require system and master group
        if (!m_system || !m_master) {
            return false;
        }

        m_busGroups.fill(nullptr);
        m_busLowPassDsps.fill(nullptr);
        m_busGroups[static_cast<size_t>(Bus::Master)] = m_master;

        const char* names[kBusCount] = { "Master", "Music", "SFX", "UI", "Ambient" };

        // Build a simple bus tree under master and attach one low-pass DSP per bus.
        for (size_t index = 0; index < kBusCount; ++index) {
            FMOD::ChannelGroup* group = nullptr;
            if (index == static_cast<size_t>(Bus::Master)) {
                group = m_master;
            }
            else {
                FMOD_RESULT result = m_system->createChannelGroup(names[index], &group);
                if (result != FMOD_OK || !group) {
                    LOG_ERROR("FMOD error (createChannelGroup): " << static_cast<int>(result));
                    return false;
                }

                result = m_master->addGroup(group, true, nullptr);
                if (result != FMOD_OK) {
                    LOG_ERROR("FMOD error (addGroup): " << static_cast<int>(result));
                    group->release();
                    return false;
                }

                m_busGroups[index] = group;
            }

            FMOD::DSP* dsp = nullptr;
            FMOD_RESULT result = m_system->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &dsp);
            if (result != FMOD_OK || !dsp) {
                LOG_ERROR("FMOD error (createDSPByType LOWPASS): " << static_cast<int>(result));
                return false;
            }

            result = group->addDSP(0, dsp);
            if (result != FMOD_OK) {
                LOG_ERROR("FMOD error (ChannelGroup::addDSP): " << static_cast<int>(result));
                dsp->release();
                return false;
            }

            m_busLowPassDsps[index] = dsp;
            _applyBusLowPassGain(static_cast<Bus>(index));
        }

        return true;
    }

    /**
     * @brief Removes and releases all bus low-pass DSP nodes and non-master channel groups.
     */
    void FmodAudioDevice::_shutdownBusRouting() {
        // release dsp objects first
        // Release DSPs first, then release non-master channel groups.
        for (size_t index = 0; index < kBusCount; ++index) {
            if (FMOD::DSP* dsp = m_busLowPassDsps[index]) {
                if (FMOD::ChannelGroup* group = m_busGroups[index]) {
                    FMOD_OK_OR_LOG(group->removeDSP(dsp), "ChannelGroup::removeDSP");
                }
                FMOD_OK_OR_LOG(dsp->release(), "DSP::release");
                m_busLowPassDsps[index] = nullptr;
            }
        }

        // release non master groups next
        for (size_t index = 0; index < kBusCount; ++index) {
            if (index == static_cast<size_t>(Bus::Master)) {
                m_busGroups[index] = m_master;
                continue;
            }

            if (FMOD::ChannelGroup* group = m_busGroups[index]) {
                group->release();
                m_busGroups[index] = nullptr;
            }
        }
    }

    /**
     * @brief Returns the FMOD channel group associated with a bus, falling back to master on invalid indices.
     * @param bus Target bus whose channel group is needed.
     * @return Pointer to the bus channel group, or the master group if the index is out of range or the group is null.
     */
    FMOD::ChannelGroup* FmodAudioDevice::_channelGroupForBus(Bus bus) const {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return m_busGroups[static_cast<size_t>(Bus::SFX)];
        }
        FMOD::ChannelGroup* group = m_busGroups[index];
        return group ? group : m_master;
    }

    /**
     * @brief Applies the stored low-pass gain and resonance to the DSP node for a bus, bypassing it when near unity.
     * @param bus Target bus whose DSP node should be updated.
     */
    void FmodAudioDevice::_applyBusLowPassGain(Bus bus) {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return;
        }

        FMOD::DSP* dsp = m_busLowPassDsps[index];
        if (!dsp) {
            return;
        }

        const float gain = m_busLowPassGain[index];
        const float cutoffHz = _lowPassGainToCutoffHz(gain);
        const float resonance = m_busLowPassResonance[index];
        // Bypass DSP near unity gain to avoid unnecessary filter work on the bus.
        FMOD_OK_OR_LOG(dsp->setBypass(gain >= 0.999f), "DSP::setBypass");
        FMOD_OK_OR_LOG(dsp->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, cutoffHz), "DSP::setParameterFloat LOWPASS_CUTOFF");
        FMOD_OK_OR_LOG(dsp->setParameterFloat(FMOD_DSP_LOWPASS_RESONANCE, resonance), "DSP::setParameterFloat LOWPASS_RESONANCE");
    }

    /**
     * @brief Clamps a low-pass gain value to [0, 1], returning 1.0 for non-finite inputs.
     * @param gain Input gain to clamp.
     * @return Clamped gain in [0, 1].
     */
    float FmodAudioDevice::_clampLowPassGain(float gain) {
        if (!std::isfinite(gain)) {
            return 1.0f;
        }
        // return clamped value
        return (gain < 0.0f) ? 0.0f : (gain > 1.0f ? 1.0f : gain);
    }

    /**
     * @brief Clamps a low-pass resonance value to the supported FMOD range, returning the minimum for non-finite inputs.
     * @param resonance Input resonance to clamp.
     * @return Clamped resonance within [kMinBusLowPassResonance, kMaxBusLowPassResonance].
     */
    float FmodAudioDevice::_clampLowPassResonance(float resonance) {
        if (!std::isfinite(resonance)) {
            return 1.0f;
        }
        return (resonance < kMinBusLowPassResonance) ? kMinBusLowPassResonance
            : (resonance > kMaxBusLowPassResonance ? kMaxBusLowPassResonance : resonance);
    }

    /**
     * @brief Maps a normalized low-pass gain [0, 1] to a cutoff frequency in Hz using exponential interpolation.
     * @param gain Normalized gain value; 0 maps to the minimum cutoff, 1 maps to the maximum cutoff.
     * @return Cutoff frequency in Hz within [kMinBusLowPassCutoffHz, kMaxBusLowPassCutoffHz].
     */
    float FmodAudioDevice::_lowPassGainToCutoffHz(float gain) {
        // clamp gain before mapping
        const float clamped = _clampLowPassGain(gain);
        if (clamped <= 0.0f) {
            return kMinBusLowPassCutoffHz;
        }
        if (clamped >= 1.0f) {
            return kMaxBusLowPassCutoffHz;
        }

        const float ratio = kMaxBusLowPassCutoffHz / kMinBusLowPassCutoffHz;
        return kMinBusLowPassCutoffHz * std::pow(ratio, clamped);
    }

} // namespace Audio
