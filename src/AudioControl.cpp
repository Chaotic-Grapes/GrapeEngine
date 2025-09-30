// audiotypes.cpp
#include "audioControl.h"
#include <string>

namespace Audio {

    // Optional helpers (useful for logs/UI)
    static const char* to_cstr(PlayMode m) {
        switch (m) {
        case PlayMode::Single:  return "Single";
        case PlayMode::Looping: return "Looping";
        }
        return "Single";
    }

    static const char* to_cstr(StopMode m) {
        switch (m) {
        case StopMode::Immediate:    return "Immediate";
        case StopMode::AllowFadeOut: return "AllowFadeOut";
        }
        return "Immediate";
    }

    // If you want string versions available elsewhere:
    std::string ToString(PlayMode m) { return to_cstr(m); }
    std::string ToString(StopMode m) { return to_cstr(m); }

} // namespace Audio
