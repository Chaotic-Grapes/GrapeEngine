/**
 * @file    ResourceManager.h
 * @author  Foo Rui Qin
 * @date    2025
 * @brief   Resource management system for caching and loading game assets
 *
 * This file defines the ResourceManager class which provides a centralized system
 * for loading, caching, and managing game assets including textures and audio files.
 * Features include:
 * - Template-based asset retrieval with automatic caching
 * - Support for multiple asset types (Texture, AudioData)
 * - Cache management utilities (clear, unload, size tracking)
 * - File existence validation and error handling
 * - Memory-efficient shared pointer usage
 * - Global singleton instance for engine-wide access
 */

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <glad/glad.h>
#include "graphics/Texture.hpp"

 // A struct is created for audio data since there's no audio class
struct AudioData {
    std::vector<uint8_t> Data;  // Raw audio file data in bytes
    std::string Path;           // Original file path of the audio asset
    std::string Format;         // Audio format extension ("wav", "mp3", "ogg", etc.)
    bool IsValid = false;       // Flag indicating if the audio data was loaded successfully
};

// Centralized resource management system for game assets
// Texture and audio
class ResourceManager {
public:
    // Default constructor
    ResourceManager() = default;

    // Default destructor
    ~ResourceManager() = default;

    // Template specializations for supported asset types
    template <typename T>
    std::shared_ptr<T> Get(const std::string& name);

    // Clear all cached assets from memory
    void ClearCache();

    // Remove a specific asset from the cache
    void UnloadAsset(const std::string& name);

    // Get the total number of cached assets
    size_t GetCacheSize() const;

    // Print detailed cache information to the log
    void PrintCacheInfo() const;

    // Check if a specific asset is already cached
    bool IsAssetCached(const std::string& name) const;

private:
    // Single generic cache map
    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<T>>& GetCacheMap();

    // Loaders (texture, audio)
    template <typename T>
    std::shared_ptr<T> Load(const std::string& filePath);

    // Asset caches (store loaded assets)
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<AudioData>> m_audioFiles;
};

// Global ResourceManager instance
extern ResourceManager RM;

#endif