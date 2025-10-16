#include "audio/Audio.h"


namespace Systems {

    void Audio::Initialize() {
        System::Trace("Initializing the interface!");
        Interface.Initialize();
    }

    void Audio::Update(float dt) {
        if (Enabled)
            Interface.Update(dt);
    }

    void Audio::Terminate() {
        Trace("Shutting down the interface...");
        Interface.Terminate();
    }

    void Audio::Add(Resources::SoundCue::Ptr soundCue) {
        Trace("'" + soundCue->getName() + "'");
        Interface.Add(soundCue);
    }

    void Audio::Add(Resources::Bank::Ptr bank) {
        Trace("'" + bank->getName() + "'");
        Interface.Add(bank);
    }

    SoundInstance::StrongPtr Audio::Play(const Resources::SoundCue::Ptr soundCue) {
        Trace("Playing '" + soundCue->getName() + "'");
        return Interface.Play(soundCue);
    }

} // namespace Systems
