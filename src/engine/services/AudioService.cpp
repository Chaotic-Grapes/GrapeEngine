#include "audio/FmodAudioDevice.h"
#include "services/AudioService.h"
#include "core/Logger.h"

namespace Services {
    void AudioService::Initialize() {
        m_device = std::make_unique<Audio::FmodAudioDevice>();

        if (!m_device || !m_device->Initialize()) {
            Trace("Audio backend failed to initialize.");
            m_device.reset();
            return;
        }

        Trace("Audio initialized: FMOD");
    }

    void AudioService::Update() {
        if (m_device && IsEnabled())
            m_device->Update();
    }

    void AudioService::Terminate() {
        if (m_device) {
            m_device->Shutdown();
            m_device.reset();
            Trace("Audio terminated.");
        }
    }
}
