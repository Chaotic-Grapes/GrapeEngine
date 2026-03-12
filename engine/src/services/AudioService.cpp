/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file    AudioService.cpp
 * @brief   Engine service that owns and drives the FMOD-backed audio device.
 *
 * @details
 * This service wraps an Audio::FmodAudioDevice instance and exposes a standard
 * engine-service lifecycle:
 *   - Initialize(): construct and initialize the FMOD device
 *   - Update():     tick the FMOD mixer each frame (if service is enabled)
 *   - Terminate():  shut down the device and release all audio resources
 *
 *
 *
 * @dependencies
 *   - audio/FmodAudioDevice.h  : concrete FMOD wrapper (Initialize/Update/Shutdown)
 *   - services/AudioService.h  : matching service interface
 *   - core/Logger.h            : Trace/LOG_* helpers (optional)
 * 
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "audio/FmodAudioDevice.h"
#include "audio/AudioEngine.h"
#include "audio/AudioCueRegistry.h"
#include "services/AudioService.h"
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "services/DeviceManager.h"
#include "core/ProjectPaths.h"
#include "services/TimeSystem.h"

Audio::FmodAudioDevice* gAudioDevice = nullptr;

namespace Services {
    /**
      * @brief Create and initialize the FMOD device. Safe to call once at startup.
      *
      * Behavior:
      * - Allocates a new FmodAudioDevice and calls its Initialize().
      * - If initialization fails, logs and resets the unique_ptr to keep state clean.
      * - On success, logs a short confirmation.
      */
    void AudioService::Initialize() {
        // Construct the device (unique ownership).
        m_device = std::make_unique<Audio::FmodAudioDevice>();

        // If allocation failed or FMOD init failed, tear down and report.
        if (!m_device || !m_device->Initialize()) {
            Trace("Audio backend failed to initialize.");
            m_device.reset(); // leave service in a safe 'no device' state
            return;
        }
        // let system activate device
        gAudioDevice = m_device.get();
        m_engine = std::make_unique<Audio::AudioEngine>(m_device.get());
        Audio::gAudioEngine = m_engine.get();

        m_cueRegistry.Refresh(Engine::ProjectPaths::GetProjectRoot());

        // FMOD system + master group are ready for use.
        Trace("Audio initialized: FMOD");

        // Subscribe to window focus changes to mute/unmute audio
        m_focusHandle = Messaging::MessageSystem::Subscribe<Messaging::WindowFocusChanged>(
            [this](const Messaging::WindowFocusChanged& event) {
                if (!m_device) return;

                if (event.HasFocus) {
                    // Window gained focus - restore previous volume
                    m_device->SetMasterVolume(m_volumeBeforeFocusLoss);
                    LOG_DEBUG("Window focused - audio volume restored to " << m_volumeBeforeFocusLoss);
                }
                else {
                    // Window lost focus - save current volume and mute
                    m_volumeBeforeFocusLoss = m_device->GetMasterVolume();
                    m_device->SetMasterVolume(0.0f);
                    LOG_DEBUG("Window unfocused - audio muted (previous volume: " << m_volumeBeforeFocusLoss << ")");
                }
            }
        );
    }

    /**
     * @brief Per-frame tick for the audio system. No-ops if service disabled.
     *
     * Notes:
     * - FMOD requires update() to be called periodically to mix audio and
     *   advance streaming/async callbacks.
     * - Guarded by IsEnabled() so you can temporarily disable the service.
     */
    void AudioService::Update() {
        if (m_device && IsEnabled()) {
            const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
            if (m_engine) {
                m_engine->Update(dt);
            }
            m_device->Update();
        }
    }

    /**
     * @brief Clean shutdown of the audio device. Safe to call multiple times.
     *
     * Behavior:
     * - Calls FmodAudioDevice::Shutdown() if the device exists
     *   (stops channels, releases sounds, closes & releases FMOD::System).
     * - Resets the unique_ptr to free the device object.
     * - Logs a short termination message.
     */
    void AudioService::Terminate() {
        // Unsubscribe from window focus events
        if (m_focusHandle.IsValid()) {
            Messaging::MessageSystem::Unsubscribe<Messaging::WindowFocusChanged>(m_focusHandle);
        }

        Audio::gAudioEngine = nullptr;
        gAudioDevice = nullptr;
        if (m_device) {
            m_device->Shutdown();
            m_device.reset();
            m_engine.reset();
            Trace("Audio terminated.");
        }
    }

    // -----------------------------------------------------------------
    // Device Management
    // -----------------------------------------------------------------

    std::vector<Engine::AudioDeviceInfo> AudioService::GetAvailableOutputDevices() const {
        return Engine::DeviceManager::EnumerateAudioOutputDevices();
    }

    std::vector<Engine::AudioDeviceInfo> AudioService::GetAvailableInputDevices() const {
        return Engine::DeviceManager::EnumerateAudioInputDevices();
    }

    Engine::AudioDeviceInfo AudioService::GetCurrentDevice() const {
        if (!m_currentAudioDeviceID.empty()) {
            // Use the direct lookup method instead of enumerating all devices
            auto device = Engine::DeviceManager::GetAudioDeviceInfo(m_currentAudioDeviceID);
            if (!device.DeviceID.empty()) {
                return device;
            }
        }
        // Return default device if current is not set or found
        return Engine::DeviceManager::GetDefaultAudioOutputDevice();
    }

    bool AudioService::SetAudioDevice(const std::string& deviceID) {
        if (!m_device) {
            LOG_ERROR("Cannot set audio device: AudioService not initialized");
            return false;
        }

        // Validate device exists and is connected
        auto devices = Engine::DeviceManager::EnumerateAudioOutputDevices();
        auto deviceIt = std::find_if(devices.begin(), devices.end(),
            [&deviceID](const auto& d) { return d.DeviceID == deviceID; });

        if (deviceIt == devices.end()) {
            LOG_ERROR("Audio device not found: " << deviceID);
            return false;
        }

        if (!deviceIt->IsConnected) {
            LOG_ERROR("Audio device is not connected: " << deviceIt->DeviceName);
            return false;
        }

        // Reinitialize FMOD with the new device
        if (_reinitializeAudioDevice(deviceID)) {
            m_currentAudioDeviceID = deviceID;
            LOG_INFO("Switched to audio device: " << deviceIt->DeviceName);
            return true;
        }

        LOG_ERROR("Failed to switch to audio device: " << deviceID);
        return false;
    }

    void AudioService::OnAudioDeviceDisconnected() {
        if (!m_device) return;

        LOG_WARNING("Audio device disconnected!");

        // Fall back to default device
        auto defaultDevice = Engine::DeviceManager::GetDefaultAudioOutputDevice();
        if (defaultDevice.DeviceID.empty()) {
            LOG_ERROR("No default audio device available!");
            return;
        }

        // Attempt to switch to default device
        if (_reinitializeAudioDevice(defaultDevice.DeviceID)) {
            m_currentAudioDeviceID = defaultDevice.DeviceID;
            LOG_INFO("Switched to default audio device: " << defaultDevice.DeviceName);
        }
        else {
            LOG_ERROR("Failed to switch to default audio device");
        }
    }

    bool AudioService::_reinitializeAudioDevice(const std::string& deviceID) {
        if (!m_device) return false;

        // Store current master volume for restoration
        float previousVolume = m_device->GetMasterVolume();

        // Stop all playing audio
        m_device->StopCue("*", Audio::StopMode::Immediate);

        // Get list of loaded cues before shutdown
        std::vector<std::pair<std::string, std::string>> loadedCues;
        m_device->GetLoadedCues(loadedCues);

        // Shutdown current FMOD system
        m_device->Shutdown();

        // Reinitialize FMOD with new device ID (passed to FmodAudioDevice)
        if (!m_device->InitializeWithDevice(deviceID)) {
            LOG_ERROR("Failed to reinitialize FMOD with device: " << deviceID);
            
            // Try to fallback to default initialization
            if (!m_device->Initialize()) {
                LOG_CRITICAL("Failed to reinitialize FMOD even with default device!");
                return false;
            }
            return true;
        }

        // Restore master volume
        m_device->SetMasterVolume(previousVolume);

        // Reload all previously loaded cues
        for (const auto& [cueId, filePath] : loadedCues) {
            // Use default params for reload; original params are lost but basic functionality restored
            if (!m_device->LoadCue(cueId, filePath, Audio::SoundParams())) {
                LOG_WARNING("Failed to reload cue after device switch: " << cueId);
            }
        }

        return true;
    }
}
