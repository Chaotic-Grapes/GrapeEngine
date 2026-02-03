/*
* @file AudioCueRegistry.h
* @brief Shared audio cue registry for consistent CueId/path resolution.
*/

#ifndef AUDIO_CUE_REGISTRY_H
#define AUDIO_CUE_REGISTRY_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "Export.h"

namespace Audio {
    class GRAPEENGINE_API AudioCueRegistry {
    public:
        struct CueInfo {
            uint32_t Id = 0;
            std::string Name;
            std::string Path;
        };

        void Refresh(const std::string& audioRoot);
        const std::vector<CueInfo>& GetAll() const { return m_cues; }
        const CueInfo* FindById(uint32_t id) const;
        const CueInfo* FindByPath(const std::string& path) const;
        const CueInfo& Register(const std::string& path);

        static std::string NormalizePath(std::string path);
        static uint32_t HashPath(const std::string& path);

    private:
        std::vector<CueInfo> m_cues;
        std::unordered_map<uint32_t, size_t> m_byId;
        std::unordered_map<std::string, size_t> m_byPath;
    };
}

#endif
