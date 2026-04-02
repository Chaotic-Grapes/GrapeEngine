/* Start Header *****************************************************************/
/*!
\file   AudioCueRegistry.h
\author Dalton Koh , 2403250
\par    d.koh@digipen.edu
\brief
Declares the shared audio cue registry used for cue id and path lookup.

Description
- stores cue metadata for id name and normalized path
- supports cue lookup by id and path
- allows refresh from an audio root folder
- provides path normalization and hashing helpers

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef AUDIO_CUE_REGISTRY_H
#define AUDIO_CUE_REGISTRY_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "Export.h"

namespace Audio {
    // shared cue registry used by audio systems and services
    class GRAPEENGINE_API AudioCueRegistry {
    public:
        // basic cue metadata stored in the registry
        struct CueInfo {
            uint32_t Id = 0;
            std::string Name;
            std::string Path;
        };

        /**
         * @brief Rebuild cue registry by scanning an audio root folder.
         * @param audioRoot Root path recursively scanned for supported audio files.
         */
        void Refresh(const std::string& audioRoot);

        /**
         * @brief Get all registered cues.
         * @return Const reference to cue metadata list.
         */
        const std::vector<CueInfo>& GetAll() const {
            // return internal cue storage
            return m_cues;
        }

        /**
         * @brief Find cue metadata by cue id.
         * @param id Cue identifier.
         * @return Pointer to cue metadata, or nullptr when id is unknown.
         */
        const CueInfo* FindById(uint32_t id) const;

        /**
         * @brief Find cue metadata by normalized path.
         * @param path Cue asset path.
         * @return Pointer to cue metadata, or nullptr when path is unknown.
         */
        const CueInfo* FindByPath(const std::string& path) const;

        /**
         * @brief Register a cue path if not present and return its metadata.
         * @param path Cue asset path to register.
         * @return Reference to existing or newly inserted cue metadata.
         */
        const CueInfo& Register(const std::string& path);

        /**
         * @brief Normalize path separators for deterministic lookup/hashing.
         * @param path Input file path.
         * @return Normalized path string.
         */
        static std::string NormalizePath(std::string path);

        /**
         * @brief Hash a normalized path into a cue id.
         * @param path Normalized cue path.
         * @return Deterministic 32-bit cue id.
         */
        static uint32_t HashPath(const std::string& path);

    private:
        // packed cue metadata storage
        std::vector<CueInfo> m_cues;
        // id to index mapping
        std::unordered_map<uint32_t, size_t> m_byId;
        // path to index mapping
        std::unordered_map<std::string, size_t> m_byPath;
    };
}

#endif
