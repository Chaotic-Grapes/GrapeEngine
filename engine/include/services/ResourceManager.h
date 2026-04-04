/* Start Header *****************************************************************/
/*!
\file   ResourceManager.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th January 2026
\brief
Defines the ResourceManager class for loading, caching and managing game
assets (textures, audio files, fonts, shaders and prefabs).

Features:
- Template-based asset retrieval with automatic caching
- Support for Texture, AudioData, Font, Shader, and PrefabData asset types
- Cache management utilities (clear, unload, size tracking)
- File validation and error handling with logging
- Global singleton instance (RM) for engine-wide access

Usage:
  auto texture = RM.Get<Texture>("assets/player.png");
  auto audio = RM.Get<AudioData>("assets/music.wav");
  auto shader = RM.Get<Shader>("assets/shaders/sprite");
  auto prefab = RM.Get<PrefabData>("assets/prefabs/enemy.prefab");

  auto font = RM.Get<Font>("assets/arial.ttf");           // Default 48px
  auto bigFont = RM.GetFont("assets/arial.ttf", 72);      // Custom size

  RM.PrintCacheInfo();
  RM.UnloadAsset("assets/player.png");
  RM.ClearCache();
*/
/* End Header *******************************************************************/

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "Export.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <glad/glad.h>
#include "graphics/Texture.hpp"
#include "graphics/Font.hpp"
#include "graphics/Shader.hpp"

// A struct is created for audio data since there's no audio class
struct AudioData {
    std::vector<uint8_t> Data;  // Raw audio file data in bytes
    std::string Path;           // Original file path of the audio asset
    std::string Format;         // Audio format extension ("wav", "mp3", "ogg", etc.)
    bool IsValid = false;       // Flag indicating if the audio data was loaded successfully
};

// Prefab data structure for storing serialized prefab content
struct PrefabData {
    std::string Path;           // Original file path of the prefab
    std::string JsonContent;    // Raw JSON content from the .prefab file
    bool IsValid = false;       // Flag indicating if the prefab was loaded successfully
};

// Raw data structure for generic file loading
// THIS IS FOR IMGUI FONT LOADING BECAUSE IT WANTS RAW BYTES
struct RawData {
    std::vector<uint8_t> Data;  // Raw file data
    std::string Path;           // File path
    bool IsValid = false;       // Success flag
};

// Centralized resource management system for game assets
// Supports textures, audio, fonts, shaders and prefabs
class GRAPEENGINE_API ResourceManager {
public:
    /**
     * @brief Construct an empty resource manager.
     */
    ResourceManager() = default;

    /**
     * @brief Destroy the resource manager.
     */
    ~ResourceManager() = default;

    /**
     * @brief List cache keys for all cached audio assets.
     * @return Vector of audio cache keys.
     */
    std::vector<std::string> ListCachedAudioPaths() const;

    /**
     * @brief Load or fetch a cached asset by path.
     * @tparam T Asset type (Texture, AudioData, Font, Shader, PrefabData, or RawData).
     * @param name Asset file path used as the cache key.
     * @return Shared pointer to the loaded asset, or nullptr on failure.
     */
    template <typename T>
    std::shared_ptr<T> Get(const std::string& name);

    /**
     * @brief Load or fetch a shader by explicit vertex/fragment paths.
     * @param vertexPath Vertex shader file path.
     * @param fragmentPath Fragment shader file path.
     * @return Shared pointer to cached or newly loaded shader.
     */
    std::shared_ptr<Shader> GetShader(const std::string& vertexPath, const std::string& fragmentPath);

    /**
     * @brief Load or fetch a font by path and pixel size.
     * @param name Font file path.
     * @param pixelSize Requested font size in pixels.
     * @return Shared pointer to cached or newly loaded font.
     */
    std::shared_ptr<Font> GetFont(const std::string& name, int pixelSize = 48);

    /**
     * @brief Clear all caches and release all loaded assets.
     */
    void ClearCache();

    /**
     * @brief Remove one asset from cache maps.
     * @param name Cache key or asset path.
     */
    void UnloadAsset(const std::string& name);

    /**
     * @brief Get total number of cached entries across asset maps.
     * @return Cache entry count.
     */
    size_t GetCacheSize() const;

    /**
     * @brief Print detailed cache statistics to the logger.
     */
    void PrintCacheInfo() const;

    /**
     * @brief Check whether an asset key exists in any cache map.
     * @param name Cache key or asset path.
     * @return True when the asset is cached.
     */
    bool IsAssetCached(const std::string& name) const;

    // File System Operations (Editor Support)
    /**
     * @brief Import an asset by copying a source file into the project.
     * @param sourcePath Source file path.
     * @param destPath Destination project-relative or absolute path.
     * @return True when copy/import succeeded.
     */
    bool AddAsset(const std::string& sourcePath, const std::string& destPath);

    /**
     * @brief Delete an asset from disk and remove it from caches.
     * @param assetPath Asset path to remove.
     * @return True when delete succeeded.
     */
    bool DeleteAsset(const std::string& assetPath);

    /**
     * @brief Replace an existing asset file and invalidate related cache entries.
     * @param assetPath Existing asset path.
     * @param newSourcePath Replacement source file path.
     * @return True when replacement succeeded.
     */
    bool ReplaceAsset(const std::string& assetPath, const std::string& newSourcePath);

    /**
     * @brief Create a directory for project asset organization.
     * @param path Directory path to create.
     * @return True when creation succeeded or directory already exists.
     */
    bool CreateDirectory(const std::string& path);

    /**
     * @brief Set the current owner tag used for subsequent asset tracking.
     * @param ownerTag Owner tag identifier.
     */
    void SetOwnerTag(const std::string& ownerTag);

    /**
     * @brief Clear the current owner tag.
     */
    void ClearOwnerTag();

    /**
     * @brief Unload assets exclusively owned by the given owner tag.
     * @param ownerTag Owner tag to remove from ownership maps.
     */
    void UnloadAssetsByOwner(const std::string& ownerTag);

private:
    /**
     * @brief Return the cache map for asset type T.
     * @tparam T Asset type.
     * @return Mutable reference to the type-specific cache map.
     */
    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<T>>& GetCacheMap();

    /**
     * @brief Load an asset from disk with validation and error handling.
     * @tparam T Asset type.
     * @param filePath Absolute or project-relative path to the asset file.
     * @return Shared pointer to the loaded asset, or nullptr on failure.
     */
    template <typename T>
    std::shared_ptr<T> Load(const std::string& filePath);

    /**
     * @brief Return the owner tracking map for asset type T.
     * @tparam T Asset type.
     * @return Mutable reference to the type-specific owner map.
     */
    template <typename T>
    std::unordered_map<std::string, std::unordered_set<std::string>>& GetOwnerMap();

    // Tracks ownership of an asset by the current owner tag
    template <typename T>
    void TrackOwner(const std::string& name) {
        if (m_ownerTag.empty()) {
            return;
        }

        // Get appropriate owner map and insert owner tag
        auto& owners = GetOwnerMap<T>();
        owners[name].insert(m_ownerTag);
    }

    // Asset caches (store loaded assets)
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<AudioData>> m_audioFiles;
    std::unordered_map<std::string, std::shared_ptr<Font>> m_fonts;
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
    std::unordered_map<std::string, std::shared_ptr<PrefabData>> m_prefabs;
    std::unordered_map<std::string, std::shared_ptr<RawData>> m_rawData;

    // Asset owners per type (asset path/cache key -> set of owner tags)
    std::unordered_map<std::string, std::unordered_set<std::string>> m_textureOwners;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_audioOwners;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_fontOwners;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_shaderOwners;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_prefabOwners;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_rawDataOwners;

    std::unordered_set<std::string> m_loggedFontCacheHits;

    std::string m_ownerTag;
};

// Global ResourceManager instance
extern GRAPEENGINE_API ResourceManager RM;

#endif
