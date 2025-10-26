/**
 * @file    ResourceManager.cpp
 * @author  Foo Rui Qin
 * @date    2025
 * @brief Implementation of the ResourceManager class for asset caching and loading
 *
 * This file implements the ResourceManager class which provides centralized asset management
 * for the game engine. Key features include:
 * - Template specializations for Texture and AudioData loading
 * - Automatic caching system to prevent redundant file operations
 * - File validation and error handling with comprehensive logging
 * - Support for multiple audio formats (WAV, MP3, OGG, FLAC, M4A, AAC)
 * - Memory-efficient binary file reading for audio assets
 * - Cache management utilities for memory optimization
 * - Integration with the engine's logging system for debugging
 */

#include "services/ResourceManager.h"
#include "core/Logger.h"
#include <fstream>
#include <algorithm>

 // Cache map accessors (template specializations)
template<>
std::unordered_map<std::string, std::shared_ptr<Texture>>&
ResourceManager::GetCacheMap<Texture>() { return m_textures; }

template<>
std::unordered_map<std::string, std::shared_ptr<AudioData>>&
ResourceManager::GetCacheMap<AudioData>() { return m_audioFiles; }

// Generic Get function
template <typename T>
std::shared_ptr<T> ResourceManager::Get(const std::string& name) {
    auto& cache = GetCacheMap<T>();

    // Check cache
    auto it = cache.find(name);
    if (it != cache.end()) {
        LOG_DEBUG("[CACHE HIT] " << typeid(T).name() << ": " << name);
        return it->second;
    }

    // Load and cache
    LOG_DEBUG("[CACHE MISS] Loading " << typeid(T).name() << ": " << name);
    auto resource = Load<T>(name);

    if (resource) {
        cache[name] = resource;
        LOG_DEBUG("Cached " << typeid(T).name() << ": " << name);
    }
    else {
        LOG_ERROR("Failed to load " << typeid(T).name() << ": " << name);
    }

    return resource;
}

// Explicit instantiations
template std::shared_ptr<Texture> ResourceManager::Get<Texture>(const std::string&);
template std::shared_ptr<AudioData> ResourceManager::Get<AudioData>(const std::string&);

// Type-specific loaders
template<>
std::shared_ptr<Texture> ResourceManager::Load<Texture>(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    try {
        auto texture = std::make_shared<Texture>(filePath);
        if (texture->ID() == 0) {
            LOG_ERROR("Texture failed to load: " << filePath);
            return nullptr;
        }
        return texture;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading texture " << filePath << ": " << e.what());
        return nullptr;
    }
}

template<>
std::shared_ptr<AudioData> ResourceManager::Load<AudioData>(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    // Validate extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    if (!extension.empty() && extension[0] == '.') extension = extension.substr(1);

    if (extension != "wav" && extension != "mp3" && extension != "ogg" &&
        extension != "flac" && extension != "m4a" && extension != "aac") {
        LOG_WARNING("Unsupported audio format: " << filePath);
        return nullptr;
    }

    // Read file
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR("Failed to open file: " << filePath);
        return nullptr;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    auto audio = std::make_shared<AudioData>();
    audio->Path = filePath;
    audio->Format = extension;
    audio->Data.resize(fileSize);

    if (!file.read(reinterpret_cast<char*>(audio->Data.data()), fileSize)) {
        LOG_ERROR("Failed to read file: " << filePath);
        return nullptr;
    }

    audio->IsValid = true;
    return audio;
}

// Clear all cached assets
void ResourceManager::ClearCache() {
    m_textures.clear();
    m_audioFiles.clear();
    LOG_INFO("Cleared all cached assets");
}

// Remove a specific asset from cache by name
void ResourceManager::UnloadAsset(const std::string& name) {
    bool removed = false;

    // Try to remove from cache
    removed |= m_textures.erase(name) > 0;
    removed |= m_audioFiles.erase(name) > 0;

    if (removed) {
        LOG_INFO("Unloaded asset: " << name);
    }
    else {
        LOG_WARNING("Asset not found in cache: " << name);
    }
}

// Get total number of cached assets
size_t ResourceManager::GetCacheSize() const {
    return m_textures.size() + m_audioFiles.size();
}

// Get cache info broken down by type
void ResourceManager::PrintCacheInfo() const {
    LOG_INFO("\nCache Info:");
    LOG_INFO("Textures: " << m_textures.size());
    LOG_INFO("Audio files: " << m_audioFiles.size());
    LOG_INFO("Total assets: " << GetCacheSize());
}

// Check if a specific asset is cached
bool ResourceManager::IsAssetCached(const std::string& name) const {
    return (m_textures.find(name) != m_textures.end()) ||
        (m_audioFiles.find(name) != m_audioFiles.end());
}

// Define the global ResourceManager instance
ResourceManager RM;
