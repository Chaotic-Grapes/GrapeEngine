#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <glad/glad.h>
#include "graphics/texture.hpp"

struct RAudio {
    std::vector<uint8_t> Data;
    int SampleRate = 0;
    int Channels = 0;      // Mono = 1, stereo = 2, etc.
    int BitsPerSample = 0; // Quality: 8-bit, 16-bit, 24-bit, etc.
    std::string Path;
    bool IsValid = false;
};

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // Main template function
    // Multiple objects use the same asset
    template <typename T>
    std::shared_ptr<T> Get(const std::string& name);

    // Utility functions
    void ClearCache();  // Empty all maps, free cached assets from memory
    void UnloadAsset(const std::string& name); // Removes 1 specific asset from cache
    size_t GetCacheSize() const;               // Total number of cached assets across all types
    void PrintCacheInfo() const;               // Breakdown by asset type
    bool IsAssetCached(const std::string& name) const;  // Check if asset's already loaded

private:
    // Asset caches (store loaded assets)
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<RAudio>> m_audioFiles;

    // Loading functions
    std::shared_ptr<Texture> _loadTexture(const std::string& filePath);
    std::shared_ptr<RAudio> _loadAudio(const std::string& filePath);
};

// Global instance
extern ResourceManager RM;

#endif