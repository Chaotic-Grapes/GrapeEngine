/*
* @file AudioCueRegistry.cpp
* @brief Shared audio cue registry implementation.
*/

#include "audio/AudioCueRegistry.h"
#include <filesystem>
#include <algorithm>
#include <functional>

namespace Audio {
    void AudioCueRegistry::Refresh(const std::string& audioRoot) {
        m_cues.clear();
        m_byId.clear();
        m_byPath.clear();

        namespace fs = std::filesystem;
        fs::path root(audioRoot);

        if (!fs::exists(root) || !fs::is_directory(root)) {
            return;
        }

        for (auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".wav" && ext != ".ogg" && ext != ".mp3" && ext != ".flac") {
                continue;
            }

            std::string path = NormalizePath(entry.path().string());
            std::string name = entry.path().stem().string();
            uint32_t id = HashPath(path);

            CueInfo info{ id, name, path };
            m_byId[id] = m_cues.size();
            m_byPath[path] = m_cues.size();
            m_cues.push_back(std::move(info));
        }
    }

    const AudioCueRegistry::CueInfo* AudioCueRegistry::FindById(uint32_t id) const {
        auto it = m_byId.find(id);
        if (it == m_byId.end()) {
            return nullptr;
        }
        return &m_cues[it->second];
    }

    const AudioCueRegistry::CueInfo* AudioCueRegistry::FindByPath(const std::string& path) const {
        std::string norm = NormalizePath(path);
        auto it = m_byPath.find(norm);
        if (it == m_byPath.end()) {
            return nullptr;
        }
        return &m_cues[it->second];
    }

    const AudioCueRegistry::CueInfo& AudioCueRegistry::Register(const std::string& path) {
        std::string norm = NormalizePath(path);
        if (auto it = m_byPath.find(norm); it != m_byPath.end()) {
            return m_cues[it->second];
        }

        CueInfo info{};
        info.Path = norm;
        info.Name = std::filesystem::path(norm).stem().string();
        info.Id = HashPath(norm);

        m_byId[info.Id] = m_cues.size();
        m_byPath[norm] = m_cues.size();
        m_cues.push_back(info);
        return m_cues.back();
    }

    std::string AudioCueRegistry::NormalizePath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    uint32_t AudioCueRegistry::HashPath(const std::string& path) {
        return static_cast<uint32_t>(std::hash<std::string>{}(path));
    }
}
