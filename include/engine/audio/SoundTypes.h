#ifndef SOUNDTYPES_H
#define SOUNDTYPES_H

namespace Audio {
    enum class PlayMode { Single, Looping };
    enum class StopMode { Immediate, AllowFadeOut };

    struct SoundParams {
        // Static cue data (loaded once)
        bool   stream = false;
        float  defaultVolume = 1.0f;
        bool   is3D = false;
    };

    struct PlaySettings {
        PlayMode mode = PlayMode::Single;
        float    volume = 1.0f;   // 0..1
        float    pitch = 1.0f;    // 0.5..2.0 etc
        bool     loop = false;    // convenience for low-level APIs
    };

    struct Vec3 { float x{}, y{}, z{}; };

    struct ListenerParams {
        Vec3 position{};
        Vec3 forward{ 0,0,-1 };
        Vec3 up{ 0,1,0 };
        Vec3 velocity{};
    };
}

#endif
