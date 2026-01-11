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

// Centralized resource management system for game assets
// Supports textures, audio, fonts, shaders and prefabs
class GRAPEENGINE_API ResourceManager {
public:
    // Default constructor
    ResourceManager() = default;

    // Default destructor
    ~ResourceManager() = default;
    
    // Storage vector for cached string paths
    std::vector<std::string> ListCachedAudioPaths() const;

    // Template function to retrieve assets with automatic caching
    template <typename T>
    std::shared_ptr<T> Get(const std::string& name);

    // Load font with specified size (allows same font at different sizes)
    std::shared_ptr<Font> GetFont(const std::string& name, int pixelSize = 48);

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
    // Returns reference to the appropriate cache map for type T
    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<T>>& GetCacheMap();

    // Loads an asset from disk with validation and error handling
    template <typename T>
    std::shared_ptr<T> Load(const std::string& filePath);

    // Asset caches (store loaded assets)
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<AudioData>> m_audioFiles;
    std::unordered_map<std::string, std::shared_ptr<Font>> m_fonts;
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
    std::unordered_map<std::string, std::shared_ptr<PrefabData>> m_prefabs;
};

// Global ResourceManager instance
extern GRAPEENGINE_API ResourceManager RM;

#endif