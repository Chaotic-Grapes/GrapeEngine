#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AudioLoader.h"   // Resources::SoundCue (your SoundCue)
#include "AudioControl.h"  // Audio::PlaybackSettings, PlayMode, StopMode

// Forward declare FMOD types so we can use pointers without including <fmod.hpp> here.
namespace FMOD { class Sound; class Channel; }

class SoundInstance {
    std::string             Name;
    Audio::PlaybackSettings Settings;

    // FMOD backend handles (set by the audio backend after play)
    FMOD::Sound* mSound = nullptr;
    FMOD::Channel* mChannel = nullptr;

public:
    // Control
    void SetParameter(std::string parameter, float value);
    void InterpolateVolume(float newVolume, float time);
    void InterpolatePitch(float newPitch, float time);
    void Resume();
    void Pause();
    void Stop(Audio::StopMode mode = Audio::StopMode::AllowFadeOut);
    bool IsPlaying();
    void SetLoop(bool enable);

    // Lifetime
    explicit SoundInstance(const Resources::SoundCue::Ptr);
    ~SoundInstance();

    // Backend binding (called by AudioFMOD right after playSound)
    void _bindBackend(FMOD::Sound* s, FMOD::Channel* c) { mSound = s; mChannel = c; }

    // Accessors
    FMOD::Sound* sound() { return mSound; }
    FMOD::Channel* channel() { return mChannel; }

    const std::string& getName() const { return Name; }
    void setName(const std::string& n) { Name = n; }

    const Audio::PlaybackSettings& getSettings() const { return Settings; }
    void setSettings(const Audio::PlaybackSettings& s) { Settings = s; }

    using Ptr = SoundInstance*;
    using StrongPtr = std::shared_ptr<SoundInstance>;
    using Container = std::vector<StrongPtr>;
};
