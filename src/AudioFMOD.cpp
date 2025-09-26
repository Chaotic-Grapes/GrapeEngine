#include "AudioFMOD.h"
#include <cassert>

static inline void FMOD_CHECK(FMOD_RESULT r) {
    assert(r == FMOD_OK);
}

bool AudioFMOD::Initialize() {
    FMOD_CHECK(FMOD::System_Create(&m_system));
    FMOD_CHECK(m_system->init(512, FMOD_INIT_NORMAL, nullptr));
    FMOD_CHECK(m_system->getMasterChannelGroup(&m_masterGroup));
    SetMasterVolume(m_masterVolume);
    return true;
}

void AudioFMOD::Update(float /*dt*/) {
    if (m_system) m_system->update();
}

void AudioFMOD::Terminate() {
    for (auto& kv : m_soundCache) {
        if (kv.second) kv.second->release();
    }
    m_soundCache.clear();

    if (m_system) {
        m_system->close();
        m_system->release();
        m_system = nullptr;
    }
    m_masterGroup = nullptr;
}

void AudioFMOD::SetMasterVolume(float v) {
    m_masterVolume = (v < 0.f) ? 0.f : (v > 1.f ? 1.f : v);
    if (m_masterGroup) m_masterGroup->setVolume(m_masterVolume);
}

void AudioFMOD::Add(Resources::SoundCue::Ptr cue) {
    // Preload into cache so first Play() is instant; otherwise Play() will create on demand
    getOrCreateSound(cue);
}

void AudioFMOD::Add(Resources::Bank::Ptr /*bank*/) {
    // If you use FMOD Studio: load banks here.
    // Low-level FMOD-only projects can leave this empty.
}

SoundInstance::StrongPtr AudioFMOD::Play(const Resources::SoundCue::Ptr cue) {
    if (!m_system) return nullptr;

    // Wrap in your SoundInstance
    auto inst = std::make_shared<SoundInstance>(cue);

    // Low-level sound
    FMOD::Sound* snd = getOrCreateSound(cue);
    if (!snd) return inst;

    // Start paused, then configure
    FMOD::Channel* ch = nullptr;
    FMOD_CHECK(m_system->playSound(snd, nullptr, true, &ch));

    // Loop
    const auto& s = cue->getSettings();
    snd->setMode(s.Loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    if (s.Loop) snd->setLoopCount(-1);

    // Volume at start
    if (ch) {
        ch->setVolume(s.Volume);
        ch->setPaused(false);
    }

    // If your Audio::Sound wrapper needs the FMOD::Channel*, set it inside inst.getSound()
    // e.g., inst->getSound().BindChannel(ch);  // depends on your wrapper

    return inst;
}

FMOD::Sound* AudioFMOD::getOrCreateSound(const Resources::SoundCue::Ptr& cue) {
    if (!m_system || !cue) return nullptr;

    const std::string key = cue->getName(); // or cue->getPath()
    auto it = m_soundCache.find(key);
    if (it != m_soundCache.end()) return it->second;

    // Decide stream vs memory based on your cue meta.
    const bool stream = cue->isStream(); // if you have this; else derive from file length or extension
    FMOD_MODE mode = FMOD_DEFAULT | (stream ? FMOD_CREATESTREAM : 0);

    FMOD::Sound* snd = nullptr;
    const std::string path = cue->getPath(); // ensure your SoundCue exposes path
    FMOD_RESULT r = m_system->createSound(path.c_str(), mode, nullptr, &snd);
    if (r != FMOD_OK) return nullptr;

    m_soundCache.emplace(key, snd);
    return snd;
}
