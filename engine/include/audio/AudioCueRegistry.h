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

        // rebuild registry by scanning the audio root folder
        void Refresh(const std::string& audioRoot);

        // return all registered cues
        const std::vector<CueInfo>& GetAll() const {
            // return internal cue storage
            return m_cues;
        }

        // find one cue by cue id
        const CueInfo* FindById(uint32_t id) const;

        // find one cue by normalized path
        const CueInfo* FindByPath(const std::string& path) const;

        // register one cue path when missing
        const CueInfo& Register(const std::string& path);

        // normalize path separators for stable lookup
        static std::string NormalizePath(std::string path);

        // hash path into a cue id value
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
