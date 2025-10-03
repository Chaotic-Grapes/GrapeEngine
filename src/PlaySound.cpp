#include "PlaySound.h"
#include <fmod.hpp>

// Construct from cue: copy name & default settings
SoundInstance::SoundInstance(const Resources::SoundCue::Ptr cue)
    : Name(cue ? cue->getName() : "")
    , Settings(cue ? cue->getSettings() : Audio::PlaybackSettings{})
{
}

SoundInstance::~SoundInstance() = default;

// Optional: map parameter names to channel properties later
void SoundInstance::SetParameter(std::string /*parameter*/, float /*value*/) {
    // placeholder for future DSP/RTPC mapping
}

void SoundInstance::InterpolateVolume(float newVolume, float /*time*/) {
    Settings.Volume = newVolume;
    if (mChannel) mChannel->setVolume(newVolume);
}

void SoundInstance::InterpolatePitch(float newPitch, float /*time*/) {
    Settings.Pitch = newPitch;
    if (mChannel) mChannel->setPitch(newPitch);
}

void SoundInstance::Resume() {
    if (mChannel) mChannel->setPaused(false);
}

void SoundInstance::Pause() {
    if (mChannel) mChannel->setPaused(true);
}

void SoundInstance::SetLoop(bool enable) {
    // remember in our settings so a future replay can inherit
    Settings.Loop = enable;

    // If currently playing, apply immediately on the channel
    if (mChannel) {
        mChannel->setMode(enable ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        mChannel->setLoopCount(enable ? -1 : 0);
    }

    // Also update the FMOD::Sound default so the next Play() inherits
    if (mSound) {
        mSound->setMode(enable ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        mSound->setLoopCount(enable ? -1 : 0);
    }
}

void SoundInstance::Stop(Audio::StopMode /*mode*/) {
    if (mChannel) {
        // If you want fades, implement a short ramp here before stop()
        mChannel->stop();
        mChannel = nullptr;
    }
}

bool SoundInstance::IsPlaying() {
    if (!mChannel) return false;
    bool playing = false;
    if (mChannel->isPlaying(&playing) != FMOD_OK) return false;
    return playing;
}
