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
#include "audio/AudioEngine.h"
#include "audio/AudioCueRegistry.h"
#include "core/messaging/MessageSystem.h"
#include "services/DeviceManager.h"

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

		// High-level audio engine (mixing/fades/policy)
		Audio::AudioEngine* Engine() { return m_engine.get(); }
		const Audio::AudioEngine* Engine() const { return m_engine.get(); }

		// Shared cue registry for CueId/path resolution
		Audio::AudioCueRegistry& CueRegistry() { return m_cueRegistry; }
		const Audio::AudioCueRegistry& CueRegistry() const { return m_cueRegistry; }


		// -----------------------------------------------------------------
		// Convenience pass-throughs (most callers do not need raw device)
		// -----------------------------------------------------------------


		// Load (or reuse) a cue by id and file path
		bool LoadCue(const std::string& cueId, const std::string& path, const Audio::SoundParams& p) const {
			return m_engine ? m_engine->LoadCue(cueId, path, p) : false;
		}


		// Play a previously-loaded cue with per-call settings
		Audio::PlaybackHandle Play(const std::string& cueId, const Audio::PlaySettings& s, Audio::Bus bus = Audio::Bus::SFX) const {
			return m_engine ? m_engine->Play(cueId, s, bus) : Audio::PlaybackHandle{};
		}


		//Stop a playing instance immediately or with a fade policy
		void Stop(const Audio::PlaybackHandle handle, Audio::StopMode mode) const { if (m_engine) m_engine->Stop(handle, mode); }

		// Pause/Resume all audio
		void PauseAll() const { if (m_device) m_device->PauseAll(); }
		void ResumeAll() const { if (m_device) m_device->ResumeAll(); }

		// Mixer controls
		void SetBusVolume(Audio::Bus bus, float volume) const { if (m_engine) m_engine->SetBusVolume(bus, volume); }
		float GetBusVolume(Audio::Bus bus) const { return m_engine ? m_engine->GetBusVolume(bus) : 1.0f; }
		void FadeBusVolume(Audio::Bus bus, float targetVolume, float duration) const { if (m_engine) m_engine->FadeBusVolume(bus, targetVolume, duration); }


		// -----------------------------------------------------------------
		// Device Management
		// -----------------------------------------------------------------

		/**
		 * @brief Get list of available output audio devices
		 * @return Vector of AudioDeviceInfo from DeviceManager
		 */
		std::vector<Engine::AudioDeviceInfo> GetAvailableOutputDevices() const;

		/**
		 * @brief Get list of available input audio devices
		 * @return Vector of AudioDeviceInfo from DeviceManager
		 */
		std::vector<Engine::AudioDeviceInfo> GetAvailableInputDevices() const;

		/**
		 * @brief Get currently active audio output device
		 * @return Current AudioDeviceInfo
		 */
		Engine::AudioDeviceInfo GetCurrentDevice() const;

		/**
		 * @brief Switch to a different audio output device
		 * 
		 * This reinitializes the FMOD system to use the new device.
		 * Stops all currently playing audio and reloads active cues.
		 * 
		 * @param deviceID Device identifier from GetAvailableOutputDevices()
		 * @return true if device switch was successful
		 */
		bool SetAudioDevice(const std::string& deviceID);

		/**
		 * @brief Handle audio device disconnection event
		 * 
		 * Called when the current audio device is disconnected.
		 * Automatically switches to system default audio device.
		 * Preserves loaded cues and attempts to resume audio.
		 */
		void OnAudioDeviceDisconnected();


	private:
		// Owns the FMOD-backed device; created in Initialize(), destroyed in Terminate().
		std::unique_ptr<Audio::FmodAudioDevice> m_device;
		std::unique_ptr<Audio::AudioEngine> m_engine;
		Audio::AudioCueRegistry m_cueRegistry;

		// Window focus handling for audio muting
		Messaging::SubscriptionHandle m_focusHandle;
		float m_volumeBeforeFocusLoss = 1.0f;

		// Device tracking
		std::string m_currentAudioDeviceID;
		
		// Helper to reinitialize FMOD with new device
		bool _reinitializeAudioDevice(const std::string& deviceID);
	};


} // namespace Services


#endif // AUDIOSERVICE_H
