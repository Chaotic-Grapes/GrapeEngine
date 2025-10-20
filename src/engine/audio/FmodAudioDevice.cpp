#include "audio/FmodAudioDevice.h"
#include "core/Logger.h"

namespace {
    bool FMOD_OK_OR_LOG(const FMOD_RESULT r, const char* ctx = nullptr) {
        if (r == FMOD_OK) return true;
        LOG_ERROR(std::string("FMOD error") + (ctx 
            ? (std::string(" (") + ctx + ")")
            : "") + ": " + std::to_string(r));
        return false;
    }
}

namespace Audio {
    bool FmodAudioDevice::Initialize() {
        if (!FMOD_OK_OR_LOG(FMOD::System_Create(&m_system), "System_Create"))
            return false;

        if (!FMOD_OK_OR_LOG(m_system->init(512, FMOD_INIT_NORMAL, nullptr), "init"))
            return false;

        if (!FMOD_OK_OR_LOG(m_system->getMasterChannelGroup(&m_master), "getMasterChannelGroup")) {
            // close/release if partially created
            if (m_system) { m_system->release(); m_system = nullptr; }
            return false;
        }

        SetMasterVolume(m_masterVolume);
        return true;
    }

    void FmodAudioDevice::Update() {
        // Update FMOD system
        if (m_system)
            m_system->update();
    }

    void FmodAudioDevice::Shutdown() {
        // release channels map pointers
        for (const auto& entry : m_channels) {
            void* data = nullptr;
            if (entry.second && entry.second->getUserData(&data) == FMOD_OK && data) {
                delete static_cast<uint64_t*>(data);
            }
        }
        m_channels.clear();

        for (auto& [id, entry] : m_cues) {
            if (entry.Sound) {
                entry.Sound->release();
                entry.Sound = nullptr;
            }
        }
        m_cues.clear();

        if (m_system) {
            m_system->close();
            m_system->release();
            m_system = nullptr;
        }
        m_master = nullptr;
    }

    bool FmodAudioDevice::LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params) {
        if (!m_system)
            return false;
        if (m_cues.count(cueId))
            return true;

        const auto* snd = _getOrCreateSound(cueId, filePath, params);

        return snd != nullptr;
    }

    void FmodAudioDevice::UnloadCue(const std::string& cueId) {
        if (const auto it = m_cues.find(cueId); it != m_cues.end()) {
            if (it->second.Sound)
                it->second.Sound->release();
            m_cues.erase(it);
        }
    }

    bool FmodAudioDevice::HasCue(const std::string& cueId) const {
        return m_cues.count(cueId) > 0;
    }

    PlaybackHandle FmodAudioDevice::Play(const std::string& cueId, const PlaySettings& s) {
        if (!m_system) 
            return {};
        const auto it = m_cues.find(cueId);
        if (it == m_cues.end()) 
            return {};
        FMOD::Sound* snd = it->second.Sound;
        if (!snd)
            return {};

        // Set looping on the sound itself (and channel)
        snd->setMode(s.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        snd->setLoopCount(s.loop ? -1 : 0);

        FMOD::Channel* ch = nullptr;
        if (!FMOD_OK_OR_LOG(m_system->playSound(snd, nullptr, true, &ch), "playSound") || !ch)
            return {};

        ch->setVolume(s.volume);
        // FMOD pitch is frequency ratio; using setPitch if Extension present, otherwise setFrequency
        ch->setPitch(s.pitch);
        ch->setPaused(false);

        PlaybackHandle h{ m_nextId++ };

        // store channel in map
        m_channels.emplace(h.Id, ch);

        // set user data to a pointer to the key
        const auto storedId = new uint64_t(h.Id);
        ch->setUserData(storedId);

        return h;
    }

    void FmodAudioDevice::Stop(const PlaybackHandle handle, const StopMode mode) {
        if (auto* ch = _channelFromHandle(handle)) {
            if (mode == StopMode::Immediate) {
                ch->stop();
            } 
            else {
                // Simple fade-out example: reduce volume and stop. Real impl should schedule.
                float v = 0.f;
                ch->getVolume(&v);
                ch->setVolume(v * 0.0f);
                ch->stop();
            }
        }
    }

    void FmodAudioDevice::SetInstanceVolume(const PlaybackHandle handle, const float volume) {
        if (auto* ch = _channelFromHandle(handle))
            ch->setVolume(volume);
    }

    void FmodAudioDevice::SetInstancePitch(const PlaybackHandle handle, const float pitch) {
        if (auto* ch = _channelFromHandle(handle))
            ch->setPitch(pitch);
    }

    void FmodAudioDevice::SetListener(const ListenerParams& l) {
        if (!m_system) 
            return;

        const FMOD_VECTOR   pos{ l.position.x, l.position.y, l.position.z },
    						vel{ l.velocity.x, l.velocity.y, l.velocity.z },
							fwd{ l.forward.x,  l.forward.y,  l.forward.z },
							up { l.up.x,       l.up.y,       l.up.z };
        m_system->set3DListenerAttributes(0, &pos, &vel, &fwd, &up);
    }

    void FmodAudioDevice::SetInstancePosition(const PlaybackHandle handle, const Vec3& pos, const Vec3& vel) {
        if (auto* ch = _channelFromHandle(handle)) {
            const FMOD_VECTOR p{ pos.x, pos.y, pos.z },
        					  v{ vel.x, vel.y, vel.z };
            ch->set3DAttributes(&p, &v);
        }
    }

    void FmodAudioDevice::SetMasterVolume(const float volume) {
        m_masterVolume = (volume < 0.f) ? 0.f : (volume > 1.f ? 1.f : volume);
        if (m_master) m_master->setVolume(m_masterVolume);
    }

    float FmodAudioDevice::GetMasterVolume() const {
        return m_masterVolume;
    }

    FMOD::Sound* FmodAudioDevice::_getOrCreateSound(const std::string& cueId, const std::string& path, const SoundParams& params) {
        if (!m_system) 
            return nullptr;
        if (const auto it = m_cues.find(cueId); it != m_cues.end())
            return it->second.Sound;

        FMOD::Sound* s = nullptr;
        auto mode = params.stream ? FMOD_CREATESTREAM : FMOD_DEFAULT;
        mode |= params.is3D ? FMOD_3D : FMOD_2D;
        const FMOD_RESULT r = m_system->createSound(path.c_str(), mode, nullptr, &s);
        
        if (r != FMOD_OK) 
            return nullptr;

        m_cues.emplace(cueId, CueEntry{ s, params });
        return s;
    }

    FMOD::Channel* FmodAudioDevice::_channelFromHandle(const PlaybackHandle h) {
        if (h.Id == 0)
            return nullptr;

        const auto it = m_channels.find(h.Id);
        if (it == m_channels.end())
            return nullptr;

        return it->second;
    }

}