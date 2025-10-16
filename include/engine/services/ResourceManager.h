/**
 * @file ResourceManager.h
 * @author Foo Rui Qin
 * @date 2024
 * @brief Resource management system for caching and loading game assets
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
#include "graphics/texture.hpp"

/**
 * @brief Audio data container for loaded audio files
 * 
 * Stores raw audio file data along with metadata for use with audio systems.
 * Supports multiple audio formats including WAV, MP3, OGG, FLAC, M4A, and AAC.
 */
struct AudioData {
    std::vector<uint8_t> Data;  ///< Raw audio file data in bytes
    std::string Path;           ///< Original file path of the audio asset
    std::string Format;         ///< Audio format extension ("wav", "mp3", "ogg", etc.)
    bool IsValid = false;       ///< Flag indicating if the audio data was loaded successfully
};

/**
 * @brief Centralized resource management system for game assets
 * 
 * The ResourceManager provides a template-based caching system for loading and managing
 * various types of game assets. It automatically handles caching to avoid redundant
 * file loading operations and provides utilities for cache management.
 * 
 * Supported asset types:
 * - Texture: Image files for rendering
 * - AudioData: Audio files for sound playback
 * 
 * Usage example:
 * @code
 * auto texture = RM.Get<Texture>("assets/player.png");
 * auto audio = RM.Get<AudioData>("assets/music.wav");
 * @endcode
 */
class ResourceManager {
public:
    /**
     * @brief Default constructor
     */
    ResourceManager() = default;
    
    /**
     * @brief Default destructor
     */
    ~ResourceManager() = default;

    /**
     * @brief Template function to retrieve assets with automatic caching
     * @tparam T The type of asset to retrieve (Texture, AudioData)
     * @param name The file path of the asset to load
     * @return std::shared_ptr<T> Shared pointer to the loaded asset, or nullptr if loading failed
     * 
     * This function first checks the cache for the requested asset. If found, returns
     * the cached version. If not found, attempts to load the asset from disk and
     * caches it for future use.
     */
    template <typename T>
    std::shared_ptr<T> Get(const std::string& name);

    /**
     * @brief Clear all cached assets from memory
     * 
     * Removes all cached textures and audio files, freeing their memory.
     * Use this to reduce memory usage when assets are no longer needed.
     */
    void ClearCache();
    
    /**
     * @brief Remove a specific asset from the cache
     * @param name The file path/name of the asset to unload
     * 
     * Searches for the asset in all cache maps and removes it if found.
     * Logs a warning if the asset is not found in any cache.
     */
    void UnloadAsset(const std::string& name);
    
    /**
     * @brief Get the total number of cached assets
     * @return size_t Total count of all cached assets across all types
     */
    size_t GetCacheSize() const;
    
    /**
     * @brief Print detailed cache information to the log
     * 
     * Outputs a breakdown of cached assets by type, including counts
     * for textures, audio files, and total assets.
     */
    void PrintCacheInfo() const;
    
    /**
     * @brief Check if a specific asset is already cached
     * @param name The file path/name of the asset to check
     * @return bool True if the asset is cached, false otherwise
     */
    bool IsAssetCached(const std::string& name) const;

private:
    // Asset caches (store loaded assets)
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;    ///< Cache for loaded texture assets
    std::unordered_map<std::string, std::shared_ptr<AudioData>> m_audioFiles; ///< Cache for loaded audio assets

    /**
     * @brief Internal function to load texture files from disk
     * @param filePath Path to the texture file
     * @return std::shared_ptr<Texture> Loaded texture or nullptr if failed
     * 
     * Validates file existence, creates a Texture object, and handles
     * loading errors with appropriate logging.
     */
    std::shared_ptr<Texture> _loadTexture(const std::string& filePath);
    
    /**
     * @brief Internal function to load audio files from disk
     * @param filePath Path to the audio file
     * @return std::shared_ptr<AudioData> Loaded audio data or nullptr if failed
     * 
     * Validates file existence and format, reads raw audio data into memory,
     * and creates an AudioData object with metadata.
     */
    std::shared_ptr<AudioData> _loadAudio(const std::string& filePath);
};

/**
 * @brief Global ResourceManager instance
 * 
 * Provides engine-wide access to the resource management system.
 * Use this instance to load and manage all game assets.
 */
extern ResourceManager RM;

#endif