#pragma once
#include "System.h"
#include "AudioBank.h"
#include "PlaySound.h"
#include "AudioControl.h"

// FMOD backend interface (provided below)
#include "AudioFMOD.h"

namespace Systems {

	class Audio : public System {

		AudioFMOD Interface;
		bool      Enabled = true;

	public:
		Audio() : System("Audio") {}

		void Initialize();
		void Update(float dt);
		void Terminate();

		// Library/resource management
		void Add(Resources::SoundCue::Ptr soundCue);
		void Add(Resources::Bank::Ptr bank);

		// Playback
		SoundInstance::StrongPtr Play(const Resources::SoundCue::Ptr soundCue);

		// Engine-level controls
		void SetEnabled(bool e) { Enabled = e; }
		bool IsEnabled() const { return Enabled; }

		// Master volume passthrough
		void SetMasterVolume(float v) { Interface.SetMasterVolume(v); }
		float GetMasterVolume() const { return Interface.GetMasterVolume(); }
	};

} // namespace Systems
