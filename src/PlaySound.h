#pragma once

#include <memory>
#include <string>
#include <vector>

// Keep this include pointing to your engine's SoundCue and low-level Audio::Sound.
// If your paths differ, fix here.
#include "../../Resources/Audio/SoundCue.h"  // Adjust if needed
#include "AudioControl.h"

// Forward declare your low-level handle type if not included by SoundCue
namespace Audio { class Sound; }

class SoundInstance {

	std::string              Name;
	Audio::PlaybackSettings  Settings;
	Audio::Sound             Sound;   // low-level object/handle (from your backend)

public:
	// Control
	void SetParameter(std::string parameter, float value);
	void InterpolateVolume(float newVolume, float time);
	void InterpolatePitch(float newPitch, float time);
	void Resume();
	void Pause();
	void Stop(Audio::StopMode mode = Audio::StopMode::AllowFadeOut);
	bool IsPlaying();

	// Lifetime
	explicit SoundInstance(const Resources::SoundCue::Ptr);
	~SoundInstance();

	// Accessors
	Audio::Sound& getSound() { return Sound; }

	// Properties (use your DEFINE_PROPERTY macros if needed)
	const std::string& getName() const { return Name; }
	void setName(const std::string& n) { Name = n; }

	const Audio::PlaybackSettings& getSettings() const { return Settings; }
	void setSettings(const Audio::PlaybackSettings& s) { Settings = s; }

	using Ptr = SoundInstance*;
	using StrongPtr = std::shared_ptr<SoundInstance>;
	using Container = std::vector<StrongPtr>;
};
