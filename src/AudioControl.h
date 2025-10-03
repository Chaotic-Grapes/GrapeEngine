#pragma once

namespace Audio {

    enum class PlayMode {
        Single,
        Looping  // fixed typo
    };

    enum class StopMode {
        Immediate,
        AllowFadeOut
    };

    struct PlaybackSettings {
        PlayMode Mode;
        float Volume, VolumeVariation;
        float Pitch, PitchVariation;
        bool  Loop;
        PlaybackSettings()
            : Mode(PlayMode::Single),
            Volume(1.0f), VolumeVariation(0.0f),
            Pitch(1.0f), PitchVariation(0.0f),
            Loop(false) {
        }
    };

} // namespace Audio
