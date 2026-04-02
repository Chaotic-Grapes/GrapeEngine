/* Start Header *****************************************************************/
/*!
\file   FmodAudioDevice.cpp
\author Dalton Koh,2403250
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
    constexpr float kMinBusLowPassCutoffHz = 20.0f;
    constexpr float kMaxBusLowPassCutoffHz = 22000.0f;
    constexpr float kMinBusLowPassResonance = 1.0f;
    constexpr float kMaxBusLowPassResonance = 10.0f;

    // The game layer often treats X/Y as the "world plane" and Z as "up"
    // (out of the screen). FMOD uses Y as up. Convert engine vectors into
    // FMOD space by swapping Y/Z and flipping X to match engine handedness.
    inline FMOD_VECTOR ToFmodVec(const Audio::Vec3& v) {
        return FMOD_VECTOR{ -v.x, v.z, v.y };
    }

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

    inline float Dot(FMOD_VECTOR a, FMOD_VECTOR b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // check fmod results and log failures
    bool FMOD_OK_OR_LOG(const FMOD_RESULT r, const char* ctx = nullptr) {
        // return true on success
        if (r == FMOD_OK) return true;
        LOG_ERROR(std::string("FMOD error") + (ctx 
            ? (std::string(" (") + ctx + ")")
            : "") + ": " + std::to_string(r));
        return false;
    }

    // read channel playing state
    inline bool _is_playing(FMOD::Channel* ch) {
        // null channels are not playing
        if (!ch) return false;
        bool playing = false;
        if (ch->isPlaying(&playing) != FMOD_OK) return false;
        return playing;
    }

    inline FMOD_MODE SpatialChannelMode(bool spatial3D) {
        if (!spatial3D) {
            return FMOD_2D;
        }
        // Linear rolloff gives predictable attenuation from min -> max distance.
        return static_cast<FMOD_MODE>(FMOD_3D | FMOD_3D_LINEARROLLOFF);
    }
}

namespace Audio {

    // initialize fmod and bus routing
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

    // initialize fmod on a requested output device
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

    // update fmod and cleanup dead single instances
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

    // release all runtime audio resources
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

    // load one cue either from memory or file path
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

    // unload one cue and clear active single map entry
    void FmodAudioDevice::UnloadCue(const std::string& cueId) {
        // release cue sound if it exists
        if (const auto it = m_cues.find(cueId); it != m_cues.end()) {
            if (it->second.Sound) it->second.Sound->release();
            m_cues.erase(it);
        }
        m_activeByCue.erase(cueId);
    }

    // check whether a cue is loaded
    bool FmodAudioDevice::HasCue(const std::string& cueId) const {
        // lookup cue map
        return m_cues.count(cueId) > 0;
    }

    // start playback for one cue
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

    // stop playback for one handle
    void FmodAudioDevice::Stop(PlaybackHandle handle, StopMode mode) {
        // resolve channel from handle
        if (auto* ch = _channelFromHandle(handle)) {
            if (mode == StopMode::Immediate) ch->stop();
            else { ch->setVolume(0.0f); ch->stop(); }
        }
    }

    // start playback with single instance policy
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

    // stop active instance tracked for one cue
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

    // query active playback for one cue
    bool FmodAudioDevice::IsCuePlaying(const std::string& cueId) const {
        // find tracked cue handle
        auto it = m_activeByCue.find(cueId);
        if (it == m_activeByCue.end()) return false;
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(PlaybackHandle{ it->second }))
            return _is_playing(ch);
        return false;
    }

    // set runtime volume for one handle
    void FmodAudioDevice::SetInstanceVolume(PlaybackHandle handle, float volume) {
        // write channel volume if handle resolves
        if (auto* ch = _channelFromHandle(handle)) ch->setVolume(volume);
    }

    // set runtime pitch for one handle
    void FmodAudioDevice::SetInstancePitch(PlaybackHandle handle, float pitch) {
        // write channel pitch if handle resolves
        if (auto* ch = _channelFromHandle(handle)) ch->setPitch(pitch);
    }

    // set runtime pan for one handle
    void FmodAudioDevice::SetInstancePan(PlaybackHandle handle, float pan) {
        // write channel pan if handle resolves
        if (auto* ch = _channelFromHandle(handle)) {
            // Pan only applies reliably to 2D channels; force 2D mode here since some cues may have
            // been created with 3D defaults even when the gameplay layer wants camera-centered panning.
            ch->setMode(FMOD_2D);
            ch->setPan(pan);
        }
    }

    // set low pass gain for one handle
    void FmodAudioDevice::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        // clamp and apply to channel low pass
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = _clampLowPassGain(gain);
            FMOD_OK_OR_LOG(ch->setLowPassGain(clamped), "Channel::setLowPassGain");
        }
    }

    // set 3d attributes for one handle
    void FmodAudioDevice::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        // write position and velocity vectors
        if (auto* ch = _channelFromHandle(handle)) {
            const FMOD_VECTOR p = ToFmodVec(pos);
            const FMOD_VECTOR v = ToFmodVec(vel);
            ch->set3DAttributes(&p, &v);
        }
    }

    void FmodAudioDevice::SetInstance3DMinMaxDistance(PlaybackHandle handle, float minDistance, float maxDistance) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float minD = minDistance < 0.0f ? 0.0f : minDistance;
            const float maxD = maxDistance < minD ? minD : maxDistance;
            FMOD_OK_OR_LOG(ch->set3DMinMaxDistance(minD, maxD), "Channel::set3DMinMaxDistance");
        }
    }

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

    void FmodAudioDevice::SetInstance3DSpread(PlaybackHandle handle, float spread) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = spread < 0.0f ? 0.0f : (spread > 360.0f ? 360.0f : spread);
            FMOD_OK_OR_LOG(ch->set3DSpread(clamped), "Channel::set3DSpread");
        }
    }

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

    void FmodAudioDevice::SetInstance3DLevel(PlaybackHandle handle, float level) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
            FMOD_OK_OR_LOG(ch->set3DLevel(clamped), "Channel::set3DLevel");
        }
    }

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

    void FmodAudioDevice::SetDefault3DMinMaxDistance(float minDistance, float maxDistance) {
        const float minD = minDistance < 0.0f ? 0.0f : minDistance;
        const float maxD = maxDistance < minD ? minD : maxDistance;
        m_default3DMinDistance = minD;
        m_default3DMaxDistance = maxD;
    }

    float FmodAudioDevice::GetDefault3DMinDistance() const {
        return m_default3DMinDistance;
    }

    float FmodAudioDevice::GetDefault3DMaxDistance() const {
        return m_default3DMaxDistance;
    }

    void FmodAudioDevice::SetDefault3DSpread(float spread) {
        m_default3DSpread = spread < 0.0f ? 0.0f : (spread > 360.0f ? 360.0f : spread);
    }

    float FmodAudioDevice::GetDefault3DSpread() const {
        return m_default3DSpread;
    }

    void FmodAudioDevice::SetDefault3DLevel(float level) {
        m_default3DLevel = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    }

    float FmodAudioDevice::GetDefault3DLevel() const {
        return m_default3DLevel;
    }

    // check if one handle is currently playing
    bool FmodAudioDevice::IsHandlePlaying(PlaybackHandle handle) const {
        // resolve mutable access for helper call
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            return _is_playing(ch);
        }
        return false;
    }

    // set bus low pass gain and apply dsp state
    void FmodAudioDevice::SetBusLowPassGain(Bus bus, float gain) {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return;
        }

        m_busLowPassGain[index] = _clampLowPassGain(gain);
        _applyBusLowPassGain(bus);
    }

    // read current bus low pass gain
    float FmodAudioDevice::GetBusLowPassGain(Bus bus) const {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return 1.0f;
        }
        return m_busLowPassGain[index];
    }

    void FmodAudioDevice::SetBusLowPassResonance(Bus bus, float resonance) {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return;
        }

        m_busLowPassResonance[index] = _clampLowPassResonance(resonance);
        _applyBusLowPassGain(bus);
    }

    float FmodAudioDevice::GetBusLowPassResonance(Bus bus) const {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return 1.0f;
        }
        return m_busLowPassResonance[index];
    }

    // set listener transform for spatial audio
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

    // set master output volume
    void FmodAudioDevice::SetMasterVolume(float volume) {
        // clamp volume to valid range
        // Clamp and apply.
        m_masterVolume = (volume < 0.f) ? 0.f : (volume > 1.f ? 1.f : volume);
        if (m_master) m_master->setVolume(m_masterVolume);
    }

    // read master output volume
    float FmodAudioDevice::GetMasterVolume() const {
        // return cached master value
        return m_masterVolume;
    }

    // pause all output through master group
    void FmodAudioDevice::PauseAll() {
        // pause master channel group
        if (m_master) m_master->setPaused(true);
    }

    // resume all output through master group
    void FmodAudioDevice::ResumeAll() {
        // resume master channel group
        if (m_master) m_master->setPaused(false);
    }

    // return list of loaded cue ids and source paths
    void FmodAudioDevice::GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const {
        // reset output container then fill
        // Return cue id -> path list.
        out.clear();
        out.reserve(m_cues.size());
        for (const auto& [id, entry] : m_cues)
            out.emplace_back(id, entry.SourcePath);
    }

    // find existing sound or create a new one from file path
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

    // resolve channel pointer from playback handle
    FMOD::Channel* FmodAudioDevice::_channelFromHandle(PlaybackHandle h) {
        // reject invalid handles
        // Resolve channel by handle id.
        if (!h) return nullptr;
        const auto it = m_channels.find(h.Id);
        if (it == m_channels.end()) return nullptr;
        return it->second;
    }

    // create sound from resource manager memory bytes
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

    // create channel groups and low pass dsp per bus
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

    // release low pass dsp and bus groups
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

    // resolve the channel group for one bus
    FMOD::ChannelGroup* FmodAudioDevice::_channelGroupForBus(Bus bus) const {
        // convert bus enum to array index
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return m_busGroups[static_cast<size_t>(Bus::SFX)];
        }
        FMOD::ChannelGroup* group = m_busGroups[index];
        return group ? group : m_master;
    }

    // apply stored low pass gain to one bus dsp
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

    // clamp low pass gain to range zero to one
    float FmodAudioDevice::_clampLowPassGain(float gain) {
        if (!std::isfinite(gain)) {
            return 1.0f;
        }
        // return clamped value
        return (gain < 0.0f) ? 0.0f : (gain > 1.0f ? 1.0f : gain);
    }

    float FmodAudioDevice::_clampLowPassResonance(float resonance) {
        if (!std::isfinite(resonance)) {
            return 1.0f;
        }
        return (resonance < kMinBusLowPassResonance) ? kMinBusLowPassResonance
            : (resonance > kMaxBusLowPassResonance ? kMaxBusLowPassResonance : resonance);
    }

    // map low pass gain to cutoff frequency
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
