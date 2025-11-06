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
 * Typical integration:
 *   1) Construct AudioService at app startup.
 *   2) Call Initialize() once. If it succeeds, the FMOD system is ready.
 *   3) Every frame, call Update() so FMOD can mix audio and process I/O.
 *   4) On shutdown, call Terminate() to clean up FMOD resources safely.
 *
 *
 * @dependencies
 *   - audio/FmodAudioDevice.h  : concrete FMOD wrapper (Initialize/Update/Shutdown)
 *   - services/AudioService.h  : matching service interface
 *   - core/Logger.h            : Trace/LOG_* helpers (optional)
 * 
 * Copyright (C) 2025 DigiPen Institute of Technology.
 * Reproduction or disclosure of this file or its contents without the
 */


#include "audio/FmodAudioDevice.h"
#include "services/AudioService.h"
#include "core/Logger.h"

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

        // Success: FMOD system + master group are ready for use.
        Trace("Audio initialized: FMOD");
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
        if (m_device && IsEnabled())
            m_device->Update();
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
        if (m_device) {
            m_device->Shutdown();
            m_device.reset();
            Trace("Audio terminated.");
        }
    }
}