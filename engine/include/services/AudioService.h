/**
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file AudioService.h
* @brief Engine service wrapper around the FMOD-based Audio::FmodAudioDevice.
*
* @details
* Provides a lightweight Service (IService) that owns and drives the FMOD
* audio device. Responsibilities:
* - Lifecycle: Initialize()/Update()/Terminate() of the underlying device
* - Pass-through helpers for common one-liners: LoadCue/Play/Stop
* - Centralized access point for audio from gameplay systems via Services API
*
* The service keeps a unique_ptr<Audio::FmodAudioDevice> and exposes a raw
* pointer getter for read-only device access when needed.
*
* @dependencies
* - core/IService.h : base service interface used by the engine
* - audio/FmodAudioDevice.h : concrete FMOD-backed audio device
* - <memory> : unique ownership of the device instance
* 
* Copyright (C) 2025 DigiPen Institute of Technology.
* Reproduction or disclosure of this file or its contents without the
*/

#ifndef AUDIOSERVICE_H
#define AUDIOSERVICE_H

#include <memory>
#include "core/IService.h"
#include "audio/FmodAudioDevice.h"
#include "core/messaging/MessageSystem.h"

namespace Services {
 
	
	//Engine-level service that manages the lifetime of the audio device
	class AudioService : public Engine::IService {
	public:
		
		// Construct the service and pass a human-readable name to base
		AudioService() : IService("Audio Service") {}



		//Ensure audio resources are released when the service is destroyed
		~AudioService() override { Terminate(); }


		// Create and initialize the FMOD device
		void Initialize() override;


		//Pump the audio device (mixing, async I/O) once per frame
		void Update() override;


		//Release the audio device and all cached sounds/channels
		void Terminate() override;


		// -----------------------------------------------------------------
		// Device accessors
		// -----------------------------------------------------------------


		// Raw device pointer for read-only use by advanced clients
		Audio::FmodAudioDevice* Device() { return m_device.get(); }

		// Const raw device pointer for read-only queries
		const Audio::FmodAudioDevice* Device() const { return m_device.get(); }


		// -----------------------------------------------------------------
		// Convenience pass-throughs (most callers do not need raw device)
		// -----------------------------------------------------------------


		// Load (or reuse) a cue by id and file path
		bool LoadCue(const std::string& cueId, const std::string& path, const Audio::SoundParams& p) const {
			return m_device->LoadCue(cueId, path, p);
		}


		// Play a previously-loaded cue with per-call settings
		Audio::PlaybackHandle Play(const std::string& cueId, const Audio::PlaySettings& s) const {
			return m_device->Play(cueId, s);
		}


		//Stop a playing instance immediately or with a fade policy
		void Stop(const Audio::PlaybackHandle handle, Audio::StopMode mode) const { m_device->Stop(handle, mode); }


	private:
		// Owns the FMOD-backed device; created in Initialize(), destroyed in Terminate().
		std::unique_ptr<Audio::FmodAudioDevice> m_device;

		// Window focus handling for audio muting
		Messaging::SubscriptionHandle m_focusHandle;
		float m_volumeBeforeFocusLoss = 1.0f;
	};


} // namespace Services


#endif // AUDIOSERVICE_H