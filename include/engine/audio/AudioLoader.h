#pragma once

#include <memory>
#include <string>
#include <utility>
#include "AudioControl.h"   // Ensure your include paths find this (e.g., added to target include dirs)

namespace Resources {

    class SoundCue {
    public:
        using Ptr = std::shared_ptr<SoundCue>;

        // Create from a file path. If 'name' is empty, it is derived from the filename.
        static Ptr CreateFromFile(std::string path, std::string name = std::string());

        // Direct constructor if you want explicit control
        explicit SoundCue(std::string path,
            std::string name,
            Audio::PlaybackSettings settings = Audio::PlaybackSettings(),
            bool isStream = true);

        // Accessors
        const std::string& getName()     const { return m_name; }
        const std::string& getPath()     const { return m_path; }
        const Audio::PlaybackSettings& getSettings() const { return m_settings; }
        bool  isStream()                 const { return m_isStream; }

        // Mutators
        void setName(std::string name) { m_name = std::move(name); }
        void setPath(std::string path) { m_path = std::move(path); }
        void setSettings(const Audio::PlaybackSettings& s) { m_settings = s; }
        void setStream(bool stream) { m_isStream = stream; }

    private:
        static std::string DeriveNameFromPath(const std::string& path);
        static bool        GuessStreamFromExtension(const std::string& path);

    private:
        std::string              m_name;
        std::string              m_path;
        Audio::PlaybackSettings  m_settings;
        bool                     m_isStream = true;
    };

} // namespace Resources