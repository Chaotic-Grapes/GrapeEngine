#pragma once

#include <fmod.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include "../../Resources/Audio/SoundCue.h"
#include "PlaySound.h"
#include "AudioControl.h"

class AudioFMOD {
public:
	AudioFMOD() = default;
	~AudioFMOD() = default;

	// Lifecycle
	bool Initialize();
	void Update(float dt);
	void Terminate();

	// Resource/Playback
	void Add(Resources::SoundCue::Ptr cue);      // loads or creates stream
	void Add(Resources::Bank::Ptr bank);         // optional, if you use FMOD Studio banks
	SoundInstance::StrongPtr Play(const Resources::SoundCue::Ptr cue);

	// Master volume
	void  SetMasterVolume(float v);
	float GetMasterVolume() const { return m_masterVolume; }

private:
	// Helpers
	FMOD::Sound* getOrCreateSound(const Resources::SoundCue::Ptr& cue);

	// FMOD state
	FMOD::System* m_system = nullptr;
	FMOD::ChannelGroup* m_masterGroup = nullptr;

	// Cache sounds by resource key/name
	std::unordered_map<std::string, FMOD::Sound*> m_soundCache;

	float m_masterVolume = 0.8f;
};
