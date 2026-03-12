/* Start Header *****************************************************************/
/*!
\file   AudioCueRegistry.cpp
\author Dalton Koh , 2403250
\par    d.koh@digipen.edu
\brief
Implements the shared audio cue registry used for cue lookup by id and path.

Description
- scans audio folders and registers supported audio files
- stores cue metadata for id name and normalized path
- supports cue lookup by id and by path
- allows runtime registration for dynamically discovered paths

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "audio/AudioCueRegistry.h"
#include <filesystem>
#include <algorithm>
#include <functional>

namespace Audio {
    // rebuild cue registry from an audio root folder
    void AudioCueRegistry::Refresh(const std::string& audioRoot) {
        // clear old registry data
        m_cues.clear();
        m_byId.clear();
        m_byPath.clear();

        // prepare filesystem handles
        namespace fs = std::filesystem;
        fs::path root(audioRoot);

        // stop when root path is invalid
        if (!fs::exists(root) || !fs::is_directory(root)) {
            return;
        }

        // scan all files under audio root
        for (auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            // keep only supported audio extensions
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".wav" && ext != ".ogg" && ext != ".mp3" && ext != ".flac") {
                continue;
            }

            // build cue metadata and store indexes
            std::string path = NormalizePath(entry.path().string());
            std::string name = entry.path().stem().string();
            uint32_t id = HashPath(path);

            CueInfo info{ id, name, path };
            m_byId[id] = m_cues.size();
            m_byPath[path] = m_cues.size();
            m_cues.push_back(std::move(info));
        }
    }

    // find cue info by cue id
    const AudioCueRegistry::CueInfo* AudioCueRegistry::FindById(uint32_t id) const {
        // lookup index by id
        auto it = m_byId.find(id);
        if (it == m_byId.end()) {
            return nullptr;
        }
        return &m_cues[it->second];
    }

    // find cue info by cue path
    const AudioCueRegistry::CueInfo* AudioCueRegistry::FindByPath(const std::string& path) const {
        // normalize input path before lookup
        std::string norm = NormalizePath(path);
        auto it = m_byPath.find(norm);
        if (it == m_byPath.end()) {
            return nullptr;
        }
        return &m_cues[it->second];
    }

    // register one cue path if not already present
    const AudioCueRegistry::CueInfo& AudioCueRegistry::Register(const std::string& path) {
        // normalize input path first
        std::string norm = NormalizePath(path);
        if (auto it = m_byPath.find(norm); it != m_byPath.end()) {
            return m_cues[it->second];
        }

        // build new cue metadata
        CueInfo info{};
        info.Path = norm;
        info.Name = std::filesystem::path(norm).stem().string();
        info.Id = HashPath(norm);

        // add cue to indexes and storage
        m_byId[info.Id] = m_cues.size();
        m_byPath[norm] = m_cues.size();
        m_cues.push_back(info);
        return m_cues.back();
    }

    // normalize path separators for stable keys
    std::string AudioCueRegistry::NormalizePath(std::string path) {
        // replace windows separators with slash
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    // hash path to stable 32 bit cue id
    uint32_t AudioCueRegistry::HashPath(const std::string& path) {
        // hash string then cast to 32 bit id
        return static_cast<uint32_t>(std::hash<std::string>{}(path));
    }
}
