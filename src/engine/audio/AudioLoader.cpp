
#include "audio/AudioLoader.h"
#include <algorithm>
#include <filesystem>   

namespace {

    namespace fs = std::filesystem;   

    static inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return s;
    }

    static inline bool EndsWith(const std::string& s, const char* suff) { // NEW (C++17)
        const size_t n = std::char_traits<char>::length(suff);
        return s.size() >= n && s.compare(s.size() - n, n, suff) == 0;
    }


    static inline std::string FilenameOnly(const std::string& path) {
        // Handle both '/' and '\'
        size_t pos1 = path.find_last_of("/\\");
        if (pos1 == std::string::npos) return path;
        return path.substr(pos1 + 1);
    }

    static inline std::string StripExtension(const std::string& filename) {
        size_t dot = filename.find_last_of('.');
        if (dot == std::string::npos) return filename;
        return filename.substr(0, dot);
    }

    static inline void trim(std::string& s) {
        auto sp = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (!s.empty() && sp(s.front())) s.erase(s.begin());
        while (!s.empty() && sp(s.back()))  s.pop_back();
    }
    static inline void strip_surrounding_quotes(std::string& s) {
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
            s = s.substr(1, s.size() - 2);
    }


} // anonymous namespace

namespace Resources {

    SoundCue::Ptr SoundCue::CreateFromFile(std::string path, std::string name) {
        if (name.empty())
            name = DeriveNameFromPath(path);

        trim(path);
        strip_surrounding_quotes(path);

        // Reject bad paths early
        if (!fs::exists(path)) {
            // std::cerr << "[Audio] Not found: " << path << "\n";
            return nullptr;
        }

        const std::string low = ToLower(path);
        const bool okExt =
            EndsWith(low, ".wav") || EndsWith(low, ".ogg") ||
            EndsWith(low, ".mp3") || EndsWith(low, ".flac") ||
            EndsWith(low, ".m4a") || EndsWith(low, ".aac");
        if (!okExt) {
            // std::cerr << "[Audio] Unsupported: " << path << "\n";
            return nullptr;
        }

        const bool stream = GuessStreamFromExtension(path);
        return std::make_shared<SoundCue>(std::move(path), std::move(name),
            Audio::PlaybackSettings{}, stream);
    }

    SoundCue::SoundCue(std::string path,
        std::string name,
        Audio::PlaybackSettings settings,
        bool isStream)
        : m_name(std::move(name))
        , m_path(std::move(path))
        , m_settings(settings)
        , m_isStream(isStream)
    {
    }

    std::string SoundCue::DeriveNameFromPath(const std::string& path) {
        return StripExtension(FilenameOnly(path));
    }

    bool SoundCue::GuessStreamFromExtension(const std::string& path) {
        const std::string low = ToLower(path);
        // Common cases:
        // - WAV is typically small/lossless for SFX: prefer non-stream (false).
        // - MP3/OGG/FLAC/etc. are often long music/ambience: prefer stream (true).
        if (low.size() >= 4 && low.substr(low.size() - 4) == ".wav")   return false;
        if (low.size() >= 4 && low.substr(low.size() - 4) == ".ogg")   return true;
        if (low.size() >= 4 && low.substr(low.size() - 4) == ".mp3")   return true;
        if (low.size() >= 5 && low.substr(low.size() - 5) == ".flac")  return true;
        if (low.size() >= 4 && low.substr(low.size() - 4) == ".aac")   return true;
        if (low.size() >= 4 && low.substr(low.size() - 4) == ".m4a")   return true;
        // Default to streaming to be safe with large files
        return true;
    }

} // namespace Resources
