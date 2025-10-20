#ifndef FMODAUDIODEVICE_H
#define FMODAUDIODEVICE_H

#include <unordered_map>
#include <string>
#include <fmod.hpp>
#include "audio/SoundTypes.h"

namespace Audio {
    struct PlaybackHandle {
        uint64_t Id = 0;
        explicit operator bool() const { return Id != 0; }
        bool operator==(const PlaybackHandle& o) const { return Id == o.Id; }
    };

    struct CueEntry {
        FMOD::Sound* Sound = nullptr;
        SoundParams  Params{};
    };

    class FmodAudioDevice final {
    public:
        FmodAudioDevice() = default;

        bool Initialize();
        void Update();
        void Shutdown();

        bool LoadCue(const std::string& cueId, const std::string& filePath, const SoundParams& params);
        void UnloadCue(const std::string& cueId);
        bool HasCue(const std::string& cueId) const;

        PlaybackHandle Play(const std::string& cueId, const PlaySettings& settings);
        void Stop(PlaybackHandle handle, StopMode mode);
        void SetInstanceVolume(PlaybackHandle handle, float volume);
        void SetInstancePitch(PlaybackHandle handle, float pitch);

        void SetListener(const ListenerParams& listener);
        void SetInstancePosition(PlaybackHandle handle, const Vec3& pos, const Vec3& vel);

        void SetMasterVolume(float volume);
        float GetMasterVolume() const;

    private:
        FMOD::System* m_system = nullptr;
        FMOD::ChannelGroup* m_master = nullptr;
        float m_masterVolume = 1.0f;

        std::unordered_map<uint64_t, FMOD::Channel*> m_channels;
        std::unordered_map<std::string, CueEntry> m_cues;
        uint64_t m_nextId = 1;

        FMOD::Sound* _getOrCreateSound(const std::string& cueId, const std::string& path, const SoundParams& params);
        FMOD::Channel* _channelFromHandle(PlaybackHandle h);
    };

}

#endif
