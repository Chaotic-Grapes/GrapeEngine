#ifndef AUDIOSERVICE_H
#define AUDIOSERVICE_H

#include <memory>
#include "core/IService.h"
#include "audio/FmodAudioDevice.h"

namespace Services {
    class AudioService : public Engine::IService {
    public:
        AudioService();
        ~AudioService() override;

        void Initialize() override;
        void Update() override;
        void Terminate() override;

        // Device passthrough (read-only for most clients)
        Audio::FmodAudioDevice* Device() { return m_device.get(); }
        const Audio::FmodAudioDevice* Device() const { return m_device.get(); }

        bool LoadCue(const std::string& cueId, const std::string& path, const Audio::SoundParams& p) const {
            return m_device->LoadCue(cueId, path, p);
        }
        Audio::PlaybackHandle Play(const std::string& cueId, const Audio::PlaySettings& s) const {
            return m_device->Play(cueId, s);
        }
        void Stop(const Audio::PlaybackHandle handle, Audio::StopMode mode) const { m_device->Stop(handle, mode); }

		std::string Name() const override { return "Audio Service"; }

    private:
        std::unique_ptr<Audio::FmodAudioDevice> m_device;
    };

}

#endif
