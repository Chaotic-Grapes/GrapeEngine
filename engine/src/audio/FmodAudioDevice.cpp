/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file    FmodAudioDevice.cpp
 *
 * @brief   FMOD-backed audio device implementation.
 */

#include "audio/FmodAudioDevice.h"
#include "core/Logger.h"
#include "services/ResourceManager.h"
#include <cmath>
#include <fmod_dsp_effects.h>

namespace {
    // Allow near-maximum LPF effect at gain 0.0f.
    constexpr float kMinBusLowPassCutoffHz = 20.0f;
    constexpr float kMaxBusLowPassCutoffHz = 22000.0f;

    // Log FMOD errors and return success/failure.
    bool FMOD_OK_OR_LOG(const FMOD_RESULT r, const char* ctx = nullptr) {
        if (r == FMOD_OK) return true;
        LOG_ERROR(std::string("FMOD error") + (ctx 
            ? (std::string(" (") + ctx + ")")
            : "") + ": " + std::to_string(r));
        return false;
    }
    // Helper to check channel playback state.
    inline bool _is_playing(FMOD::Channel* ch) {
        if (!ch) return false;
        bool playing = false;
        if (ch->isPlaying(&playing) != FMOD_OK) return false;
        return playing;
    }
}

namespace Audio {

    bool FmodAudioDevice::Initialize() {
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

    bool FmodAudioDevice::InitializeWithDevice(const std::string& deviceID) {
        m_busGroups.fill(nullptr);
        m_busLowPassDsps.fill(nullptr);

        // Create FMOD system first.
        if (!FMOD_OK_OR_LOG(FMOD::System_Create(&m_system), "System_Create"))
            return false;

        // Convert device ID string to integer index
        // Device IDs from DeviceManager are typically formatted as numeric strings or device names
        int driverIndex = 0;
        try {
            // Try to parse as integer first (0, 1, 2, etc.)
            driverIndex = std::stoi(deviceID);
        }
        catch (...) {
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

    void FmodAudioDevice::Update() {
        // Pump FMOD update.
        if (m_system) m_system->update();

        // Prune finished singletons.
        for (auto it = m_activeByCue.begin(); it != m_activeByCue.end(); ) {
            FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second });
            if (!ch || !_is_playing(ch)) it = m_activeByCue.erase(it);
            else ++it;
        }
    }

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

    bool FmodAudioDevice::LoadCue(const std::string& cueId,
        const std::string& filePath,
        const SoundParams& params)
    {
        if (!m_system) return false;
        if (m_cues.count(cueId)) return true; // already loaded

        // Try ResourceManager memory path.
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

        // Fallback: create from file path.
        FMOD::Sound* s = nullptr;
        auto mode = params.Stream ? FMOD_CREATESTREAM : FMOD_DEFAULT;
        mode |= params.Is3D ? FMOD_3D : FMOD_2D;
        if (m_system->createSound(filePath.c_str(), mode, nullptr, &s) != FMOD_OK || !s)
            return false;

        m_cues.emplace(cueId, CueEntry{ s, params, filePath });
        return true;
    }

    void FmodAudioDevice::UnloadCue(const std::string& cueId) {
        if (const auto it = m_cues.find(cueId); it != m_cues.end()) {
            if (it->second.Sound) it->second.Sound->release();
            m_cues.erase(it);
        }
        m_activeByCue.erase(cueId);
    }

    bool FmodAudioDevice::HasCue(const std::string& cueId) const {
        return m_cues.count(cueId) > 0;
    }

    PlaybackHandle FmodAudioDevice::Play(const std::string& cueId,
        const PlaySettings& s,
        Bus bus)
    {
        const auto it = m_cues.find(cueId);
        if (it == m_cues.end()) return {};

        FMOD::Sound* snd = it->second.Sound;
        if (!snd) return {};

        FMOD::ChannelGroup* targetGroup = _channelGroupForBus(bus);
        if (!targetGroup) {
            targetGroup = m_master;
        }

        // Configure looping on the sound.
        snd->setMode(s.Loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        snd->setLoopCount(s.Loop ? -1 : 0);

        FMOD::Channel* ch = nullptr;
        if (!FMOD_OK_OR_LOG(m_system->playSound(snd, targetGroup, true, &ch), "playSound") || !ch)
            return {};

        // Apply channel settings.
        ch->setMode(s.Spatial3D ? FMOD_3D : FMOD_2D);
        if (s.Spatial3D) {
            // Default falloff tuned for small scenes; adjust if needed.
            ch->set3DMinMaxDistance(1.0f, 25.0f);
        }
        ch->setVolume(s.Volume);
        ch->setPitch(s.Pitch);
        if (!s.Spatial3D) {
            ch->setPan(s.Pan);
        }
        ch->setPaused(s.StartPaused);

        // Track playback handle.
        PlaybackHandle h{ m_nextId++ };
        m_channels.emplace(h.Id, ch);
        auto* storedId = new uint64_t(h.Id);
        ch->setUserData(storedId);
        return h;
    }

    void FmodAudioDevice::Stop(PlaybackHandle handle, StopMode mode) {
        if (auto* ch = _channelFromHandle(handle)) {
            if (mode == StopMode::Immediate) ch->stop();
            else { ch->setVolume(0.0f); ch->stop(); }
        }
    }

    PlaybackHandle FmodAudioDevice::PlaySingle(const std::string& cueId,
        const PlaySettings& s,
        PlayPolicy policy,
        Bus bus)
    {
        FMOD::ChannelGroup* targetGroup = _channelGroupForBus(bus);
        if (!targetGroup) {
            targetGroup = m_master;
        }

        if (auto it = m_activeByCue.find(cueId); it != m_activeByCue.end()) {
            if (FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second })) {
                const bool playing = _is_playing(ch);
                switch (policy) {
                case PlayPolicy::SingleInstanceRestart:
                    ch->setPosition(0, FMOD_TIMEUNIT_MS);
                    ch->setChannelGroup(targetGroup);
                    ch->setMode(s.Spatial3D ? FMOD_3D : FMOD_2D);
                    if (s.Spatial3D) {
                        // Default falloff tuned for small scenes; adjust if needed.
                        ch->set3DMinMaxDistance(1.0f, 25.0f);
                    }
                    if (!s.Spatial3D) {
                        ch->setPan(s.Pan);
                    }
                    ch->setPaused(s.StartPaused ? true : false);
                    ch->setVolume(s.Volume);
                    ch->setPitch(s.Pitch);
                    return PlaybackHandle{ it->second };
                case PlayPolicy::SingleInstanceResume:
                    if (!playing && !s.StartPaused) ch->setPaused(false);
                    if (s.StartPaused) ch->setPaused(true);
                    ch->setChannelGroup(targetGroup);
                    ch->setMode(s.Spatial3D ? FMOD_3D : FMOD_2D);
                    if (s.Spatial3D) {
                        // Default falloff tuned for small scenes; adjust if needed.
                        ch->set3DMinMaxDistance(1.0f, 25.0f);
                    }
                    if (!s.Spatial3D) {
                        ch->setPan(s.Pan);
                    }
                    ch->setVolume(s.Volume);
                    ch->setPitch(s.Pitch);
                    return PlaybackHandle{ it->second };
                case PlayPolicy::SingleInstanceIgnore:
                    if (playing) return PlaybackHandle{ it->second };
                    break;
                case PlayPolicy::NewInstance:
                    break;
                }
            }
            else {
                m_activeByCue.erase(it);
            }
        }
        auto h = Play(cueId, s, bus);
        if (h) m_activeByCue[cueId] = h.Id;
        return h;
    }

    void FmodAudioDevice::StopCue(const std::string& cueId, StopMode mode) {
        auto it = m_activeByCue.find(cueId);
        if (it == m_activeByCue.end()) return;
        if (FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second })) {
            if (mode == StopMode::Immediate) ch->stop();
            else { ch->setVolume(0.0f); ch->stop(); }
        }
        m_activeByCue.erase(it);
    }

    bool FmodAudioDevice::IsCuePlaying(const std::string& cueId) const {
        auto it = m_activeByCue.find(cueId);
        if (it == m_activeByCue.end()) return false;
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(PlaybackHandle{ it->second }))
            return _is_playing(ch);
        return false;
    }

    void FmodAudioDevice::SetInstanceVolume(PlaybackHandle handle, float volume) {
        if (auto* ch = _channelFromHandle(handle)) ch->setVolume(volume);
    }

    void FmodAudioDevice::SetInstancePitch(PlaybackHandle handle, float pitch) {
        if (auto* ch = _channelFromHandle(handle)) ch->setPitch(pitch);
    }

    void FmodAudioDevice::SetInstancePan(PlaybackHandle handle, float pan) {
        if (auto* ch = _channelFromHandle(handle)) ch->setPan(pan);
    }

    void FmodAudioDevice::SetInstanceLowPassGain(PlaybackHandle handle, float gain) {
        if (auto* ch = _channelFromHandle(handle)) {
            const float clamped = _clampLowPassGain(gain);
            FMOD_OK_OR_LOG(ch->setLowPassGain(clamped), "Channel::setLowPassGain");
        }
    }

    void FmodAudioDevice::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        if (auto* ch = _channelFromHandle(handle)) {
            const FMOD_VECTOR p{ pos.x, pos.y, pos.z };
            const FMOD_VECTOR v{ vel.x, vel.y, vel.z };
            ch->set3DAttributes(&p, &v);
        }
    }

    bool FmodAudioDevice::IsHandlePlaying(PlaybackHandle handle) const {
        auto* self = const_cast<FmodAudioDevice*>(this);
        if (FMOD::Channel* ch = self->_channelFromHandle(handle)) {
            return _is_playing(ch);
        }
        return false;
    }

    void FmodAudioDevice::SetBusLowPassGain(Bus bus, float gain) {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return;
        }

        m_busLowPassGain[index] = _clampLowPassGain(gain);
        _applyBusLowPassGain(bus);
    }

    float FmodAudioDevice::GetBusLowPassGain(Bus bus) const {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return 1.0f;
        }
        return m_busLowPassGain[index];
    }

    void FmodAudioDevice::SetListener(const ListenerParams& l) {
        if (!m_system) return;
        // Update listener attributes.
        const FMOD_VECTOR pos{ l.Position.x, l.Position.y, l.Position.z };
        const FMOD_VECTOR vel{ l.Velocity.x, l.Velocity.y, l.Velocity.z };
        const FMOD_VECTOR fwd{ l.Forward.x, l.Forward.y, l.Forward.z };
        const FMOD_VECTOR up{ l.Up.x,      l.Up.y,      l.Up.z };
        m_system->set3DListenerAttributes(0, &pos, &vel, &fwd, &up);
    }

    void FmodAudioDevice::SetMasterVolume(float volume) {
        // Clamp and apply.
        m_masterVolume = (volume < 0.f) ? 0.f : (volume > 1.f ? 1.f : volume);
        if (m_master) m_master->setVolume(m_masterVolume);
    }

    float FmodAudioDevice::GetMasterVolume() const {
        return m_masterVolume;
    }

    void FmodAudioDevice::PauseAll() {
        if (m_master) m_master->setPaused(true);
    }

    void FmodAudioDevice::ResumeAll() {
        if (m_master) m_master->setPaused(false);
    }

    void FmodAudioDevice::GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const {
        // Return cue id -> path list.
        out.clear();
        out.reserve(m_cues.size());
        for (const auto& [id, entry] : m_cues)
            out.emplace_back(id, entry.SourcePath);
    }

    FMOD::Sound* FmodAudioDevice::_getOrCreateSound(const std::string& cueId,
        const std::string& path,
        const SoundParams& params)
    {
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

    FMOD::Channel* FmodAudioDevice::_channelFromHandle(PlaybackHandle h) {
        // Resolve channel by handle id.
        if (!h) return nullptr;
        const auto it = m_channels.find(h.Id);
        if (it == m_channels.end()) return nullptr;
        return it->second;
    }

    FMOD::Sound* FmodAudioDevice::_createSoundFromMemory(const std::string& cueId,
        const std::string& path,
        const SoundParams& params)
    {
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

    bool FmodAudioDevice::_initializeBusRouting() {
        if (!m_system || !m_master) {
            return false;
        }

        m_busGroups.fill(nullptr);
        m_busLowPassDsps.fill(nullptr);
        m_busGroups[static_cast<size_t>(Bus::Master)] = m_master;

        const char* names[kBusCount] = { "Master", "Music", "SFX", "UI", "Ambient" };

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

    void FmodAudioDevice::_shutdownBusRouting() {
        for (size_t index = 0; index < kBusCount; ++index) {
            if (FMOD::DSP* dsp = m_busLowPassDsps[index]) {
                if (FMOD::ChannelGroup* group = m_busGroups[index]) {
                    group->removeDSP(dsp);
                }
                dsp->release();
                m_busLowPassDsps[index] = nullptr;
            }
        }

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

    FMOD::ChannelGroup* FmodAudioDevice::_channelGroupForBus(Bus bus) const {
        const size_t index = static_cast<size_t>(bus);
        if (index >= kBusCount) {
            return m_busGroups[static_cast<size_t>(Bus::SFX)];
        }
        FMOD::ChannelGroup* group = m_busGroups[index];
        return group ? group : m_master;
    }

    void FmodAudioDevice::_applyBusLowPassGain(Bus bus) {
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
        dsp->setBypass(gain >= 0.999f);
        FMOD_OK_OR_LOG(dsp->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, cutoffHz), "DSP::setParameterFloat LOWPASS_CUTOFF");
    }

    float FmodAudioDevice::_clampLowPassGain(float gain) {
        return (gain < 0.0f) ? 0.0f : (gain > 1.0f ? 1.0f : gain);
    }

    float FmodAudioDevice::_lowPassGainToCutoffHz(float gain) {
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
