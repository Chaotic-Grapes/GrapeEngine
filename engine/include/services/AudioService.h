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
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
		/**
		 * @brief Construct the audio service.
		 */
		AudioService() : IService("Audio Service") {}



		/**
		 * @brief Destroy the service and release owned audio resources.
		 */
		~AudioService() override { Terminate(); }


		/**
		 * @brief Initialize audio device, engine, and runtime wiring.
		 */
		void Initialize() override;


		/**
		 * @brief Run one audio tick for mixing, fades, and device updates.
		 */
		void Update() override;


		/**
		 * @brief Terminate audio runtime and release device resources.
		 */
		void Terminate() override;


		// Device accessors



		/**
		 * @brief Access the underlying FMOD device.
		 * @return Mutable device pointer, or nullptr when uninitialized.
		 */
		Audio::FmodAudioDevice* Device() { return m_device.get(); }

		/**
		 * @brief Access the underlying FMOD device (const).
		 * @return Const device pointer, or nullptr when uninitialized.
		 */
		const Audio::FmodAudioDevice* Device() const { return m_device.get(); }

		/**
		 * @brief Access the high-level audio engine facade.
		 * @return Mutable audio engine pointer, or nullptr when uninitialized.
		 */
		Audio::AudioEngine* Engine() { return m_engine.get(); }

		/**
		 * @brief Access the high-level audio engine facade (const).
		 * @return Const audio engine pointer, or nullptr when uninitialized.
		 */
		const Audio::AudioEngine* Engine() const { return m_engine.get(); }

		/**
		 * @brief Access the shared cue registry.
		 * @return Mutable cue registry reference.
		 */
		Audio::AudioCueRegistry& CueRegistry() { return m_cueRegistry; }

		/**
		 * @brief Access the shared cue registry (const).
		 * @return Const cue registry reference.
		 */
		const Audio::AudioCueRegistry& CueRegistry() const { return m_cueRegistry; }


		// Convenience pass-throughs (most callers do not need raw device)
	


		/**
		 * @brief Load or reuse a cue and associate it with a cue id.
		 * @param cueId Logical cue identifier.
		 * @param path Asset path to audio data.
		 * @param p Sound loading parameters.
		 * @return True when cue load or reuse succeeds.
		 */
		bool LoadCue(const std::string& cueId, const std::string& path, const Audio::SoundParams& p) const {
			return m_engine ? m_engine->LoadCue(cueId, path, p) : false;
		}


		/**
		 * @brief Play a previously loaded cue.
		 * @param cueId Cue identifier to play.
		 * @param s Per-playback settings.
		 * @param bus Mixer bus route for the playback.
		 * @return Playback handle for subsequent control.
		 */
		Audio::PlaybackHandle Play(const std::string& cueId, const Audio::PlaySettings& s, Audio::Bus bus = Audio::Bus::SFX) const {
			return m_engine ? m_engine->Play(cueId, s, bus) : Audio::PlaybackHandle{};
		}


		/**
		 * @brief Stop an active playback instance.
		 * @param handle Playback handle returned by Play.
		 * @param mode Stop behavior (immediate or fade out).
		 */
		void Stop(const Audio::PlaybackHandle handle, Audio::StopMode mode) const { if (m_engine) m_engine->Stop(handle, mode); }

		/**
		 * @brief Pause all active audio output.
		 */
		void PauseAll() const { if (m_device) m_device->PauseAll(); }

		/**
		 * @brief Resume all previously paused audio output.
		 */
		void ResumeAll() const { if (m_device) m_device->ResumeAll(); }

		/**
		 * @brief Set bus output volume.
		 * @param bus Target mixer bus.
		 * @param volume Linear gain value.
		 */
		void SetBusVolume(Audio::Bus bus, float volume) const { if (m_engine) m_engine->SetBusVolume(bus, volume); }

		/**
		 * @brief Get current bus output volume.
		 * @param bus Target mixer bus.
		 * @return Linear gain value for the bus.
		 */
		float GetBusVolume(Audio::Bus bus) const { return m_engine ? m_engine->GetBusVolume(bus) : 1.0f; }

		/**
		 * @brief Fade bus output volume over time.
		 * @param bus Target mixer bus.
		 * @param targetVolume Destination linear gain.
		 * @param duration Fade duration in seconds.
		 */
		void FadeBusVolume(Audio::Bus bus, float targetVolume, float duration) const { if (m_engine) m_engine->FadeBusVolume(bus, targetVolume, duration); }

		/**
		 * @brief Set low-pass gain on a bus.
		 * @param bus Target mixer bus.
		 * @param gain Low-pass gain factor.
		 */
		void SetBusLowPassGain(Audio::Bus bus, float gain) const { if (m_engine) m_engine->SetBusLowPassGain(bus, gain); }

		/**
		 * @brief Get low-pass gain on a bus.
		 * @param bus Target mixer bus.
		 * @return Low-pass gain factor.
		 */
		float GetBusLowPassGain(Audio::Bus bus) const { return m_engine ? m_engine->GetBusLowPassGain(bus) : 1.0f; }



		// Device Management


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
