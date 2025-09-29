
#pragma once

#include "ecs/ISystem.h"
#include "../src/Audio.h"  // your Systems::Audio facade (wraps AudioFMOD)

namespace Engine {

    class AudioSystem final : public Engine::ISystem {
    public:

        AudioSystem() = default;

        void OnCreate() override { m_audio.Initialize(); }
        void OnUpdate() override { m_audio.Update(0.0f); } // pass dt here if you have it
        ~AudioSystem() override { m_audio.Terminate(); }

        std::string Name() const override { return "AudioSystem"; }

        // Expose the facade so Overlay / DebugUI can use it
        Systems::Audio* GetAudio() { return &m_audio; }

    private:
        Systems::Audio m_audio; // owns the facade; World owns this system
    };
}