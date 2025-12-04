#include "audio/FmodAudioDevice.h"
#include "core/Logger.h"
#include "services/ResourceManager.h"

namespace {
    bool FMOD_OK_OR_LOG(const FMOD_RESULT r, const char* ctx = nullptr) {
        if (r == FMOD_OK) return true;
        LOG_ERROR(std::string("FMOD error") + (ctx 
            ? (std::string(" (") + ctx + ")")
            : "") + ": " + std::to_string(r));
        return false;
    }
    inline bool _is_playing(FMOD::Channel* ch) {
        if (!ch) return false;
        bool playing = false;
        if (ch->isPlaying(&playing) != FMOD_OK) return false;
        return playing;
    }
}

namespace Audio {

    bool FmodAudioDevice::Initialize() {
        if (!FMOD_OK_OR_LOG(FMOD::System_Create(&m_system), "System_Create"))
            return false;
        if (!FMOD_OK_OR_LOG(m_system->init(512, FMOD_INIT_NORMAL, nullptr), "init"))
            return false;
        if (!FMOD_OK_OR_LOG(m_system->getMasterChannelGroup(&m_master), "getMasterChannelGroup")) {
            if (m_system) { m_system->release(); m_system = nullptr; }
            return false;
        }
        SetMasterVolume(m_masterVolume);
        return true;
    }

    void FmodAudioDevice::Update() {
        if (m_system) m_system->update();

        // prune finished singletons
        for (auto it = m_activeByCue.begin(); it != m_activeByCue.end(); ) {
            FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second });
            if (!ch || !_is_playing(ch)) it = m_activeByCue.erase(it);
            else ++it;
        }
    }

    void FmodAudioDevice::Shutdown() {
        // free channel user data
        for (const auto& [id, ch] : m_channels) {
            void* data = nullptr;
            if (ch && ch->getUserData(&data) == FMOD_OK && data) {
                delete static_cast<uint64_t*>(data);
            }
        }
        m_channels.clear();
        m_activeByCue.clear();

        // release sounds
        for (auto& [cid, entry] : m_cues) {
            if (entry.Sound) { entry.Sound->release(); entry.Sound = nullptr; }
        }
        m_cues.clear();

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

        // Try ResourceManager memory path
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

        // Fallback: create from file path
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
        const PlaySettings& s)
    {
        const auto it = m_cues.find(cueId);
        if (it == m_cues.end()) return {};

        FMOD::Sound* snd = it->second.Sound;
        if (!snd) return {};

        // Configure looping on the sound
        snd->setMode(s.Loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        snd->setLoopCount(s.Loop ? -1 : 0);

        FMOD::Channel* ch = nullptr;
        if (!FMOD_OK_OR_LOG(m_system->playSound(snd, nullptr, true, &ch), "playSound") || !ch)
            return {};

        ch->setVolume(s.Volume);
        ch->setPitch(s.Pitch);
        ch->setPaused(false);

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
        PlayPolicy policy)
    {
        if (auto it = m_activeByCue.find(cueId); it != m_activeByCue.end()) {
            if (FMOD::Channel* ch = _channelFromHandle(PlaybackHandle{ it->second })) {
                const bool playing = _is_playing(ch);
                switch (policy) {
                case PlayPolicy::SingleInstanceRestart:
                    ch->setPosition(0, FMOD_TIMEUNIT_MS);
                    ch->setPaused(false);
                    ch->setVolume(s.Volume);
                    ch->setPitch(s.Pitch);
                    return PlaybackHandle{ it->second };
                case PlayPolicy::SingleInstanceResume:
                    if (!playing) ch->setPaused(false);
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
        auto h = Play(cueId, s);
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

    void FmodAudioDevice::SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        if (auto* ch = _channelFromHandle(handle)) {
            const FMOD_VECTOR p{ pos.x, pos.y, pos.z };
            const FMOD_VECTOR v{ vel.x, vel.y, vel.z };
            ch->set3DAttributes(&p, &v);
        }
    }

    void FmodAudioDevice::SetListener(const ListenerParams& l) {
        if (!m_system) return;
        const FMOD_VECTOR pos{ l.Position.x, l.Position.y, l.Position.z };
        const FMOD_VECTOR vel{ l.Velocity.x, l.Velocity.y, l.Velocity.z };
        const FMOD_VECTOR fwd{ l.Forward.x, l.Forward.y, l.Forward.z };
        const FMOD_VECTOR up{ l.Up.x,      l.Up.y,      l.Up.z };
        m_system->set3DListenerAttributes(0, &pos, &vel, &fwd, &up);
    }

    void FmodAudioDevice::SetMasterVolume(float volume) {
        m_masterVolume = (volume < 0.f) ? 0.f : (volume > 1.f ? 1.f : volume);
        if (m_master) m_master->setVolume(m_masterVolume);
    }

    float FmodAudioDevice::GetMasterVolume() const {
        return m_masterVolume;
    }

    void FmodAudioDevice::GetLoadedCues(std::vector<std::pair<std::string, std::string>>& out) const {
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

        FMOD::Sound* s = nullptr;
        auto mode = params.Stream ? FMOD_CREATESTREAM : FMOD_DEFAULT;
        mode |= params.Is3D ? FMOD_3D : FMOD_2D;
        if (m_system->createSound(path.c_str(), mode, nullptr, &s) != FMOD_OK || !s)
            return nullptr;

        m_cues.emplace(cueId, CueEntry{ s, params, path });
        return s;
    }

    FMOD::Channel* FmodAudioDevice::_channelFromHandle(PlaybackHandle h) {
        if (!h) return nullptr;
        const auto it = m_channels.find(h.Id);
        if (it == m_channels.end()) return nullptr;
        return it->second;
    }

    FMOD::Sound* FmodAudioDevice::_createSoundFromMemory(const std::string& cueId,
        const std::string& path,
        const SoundParams& params)
    {
        (void)cueId;

        auto audioBytes = RM.Get<AudioData>(path);
        if (!audioBytes || !audioBytes->IsValid || audioBytes->Data.empty())
            return nullptr;

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

} // namespace Audio