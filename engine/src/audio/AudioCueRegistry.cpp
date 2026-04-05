/* Start Header *****************************************************************/
/*!
\file   AudioCueRegistry.cpp
\author Dalton Koh (100%)
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
    /**
     * @brief Scans an audio root folder and rebuilds the cue registry with all supported audio files found.
     * @param audioRoot Filesystem path to the root directory to scan recursively.
     */
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

    /**
     * @brief Looks up a registered cue by its hashed numeric identifier.
     * @param id 32-bit hash id of the cue to find.
     * @return Pointer to the matching CueInfo, or nullptr if not found.
     */
    const AudioCueRegistry::CueInfo* AudioCueRegistry::FindById(uint32_t id) const {
        // lookup index by id
        auto it = m_byId.find(id);
        if (it == m_byId.end()) {
            return nullptr;
        }
        return &m_cues[it->second];
    }

    /**
     * @brief Looks up a registered cue by its normalized file path.
     * @param path File path of the cue; separators are normalized before lookup.
     * @return Pointer to the matching CueInfo, or nullptr if not found.
     */
    const AudioCueRegistry::CueInfo* AudioCueRegistry::FindByPath(const std::string& path) const {
        // normalize input path before lookup
        std::string norm = NormalizePath(path);
        auto it = m_byPath.find(norm);
        if (it == m_byPath.end()) {
            return nullptr;
        }
        return &m_cues[it->second];
    }

    /**
     * @brief Registers a cue path in the registry, creating a new entry if it does not already exist.
     * @param path File path of the cue to register.
     * @return Reference to the existing or newly created CueInfo for the given path.
     */
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

    /**
     * @brief Normalizes all backslash separators in a path to forward slashes for consistent map keys.
     * @param path Input path string to normalize.
     * @return Path string with all backslashes replaced by forward slashes.
     */
    std::string AudioCueRegistry::NormalizePath(std::string path) {
        // replace windows separators with slash
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    /**
     * @brief Hashes a normalized path string into a stable 32-bit cue identifier.
     * @param path Normalized path string to hash.
     * @return 32-bit hash value used as the cue's numeric id.
     */
    uint32_t AudioCueRegistry::HashPath(const std::string& path) {
        // hash string then cast to 32 bit id
        return static_cast<uint32_t>(std::hash<std::string>{}(path));
    }
}
