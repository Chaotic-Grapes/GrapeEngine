/* Start Header *****************************************************************/
/*!
\file   ResourceManager.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the ResourceManager class for centralized asset management.

Features:
- Template specializations for Texture and AudioData loading
- Automatic caching system to prevent redundant file operations
- File validation and error handling with logging
- Support for multiple audio formats (WAV, MP3, OGG, FLAC, M4A, AAC)
- Cache management utilities for memory optimization
*/
/* End Header *******************************************************************/

#include "services/ResourceManager.h"
#include "core/Logger.h"
#include <fstream>
#include <algorithm>

// Cache map accessors (template specializations)
// When T = Texture, return texture cache
template<>
std::unordered_map<std::string, std::shared_ptr<Texture>>&
ResourceManager::GetCacheMap<Texture>() { return m_textures; }

// When T = AudioData, return audio cache
template<>
std::unordered_map<std::string, std::shared_ptr<AudioData>>&
ResourceManager::GetCacheMap<AudioData>() { return m_audioFiles; }

// Generic Get function: handles caching logic for all asset types
template <typename T>
std::shared_ptr<T> ResourceManager::Get(const std::string& name) {
    // Get reference to appropriate cache (m_textures or m_audioFiles)
    auto& cache = GetCacheMap<T>();

    // Check cache first
    auto it = cache.find(name);
    if (it != cache.end()) {
        LOG_DEBUG("[CACHE HIT] " << typeid(T).name() << ": " << name);
        return it->second;  // Cache hit
    }

    // Not in cache: try to load
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
// When other files call RM.Get<T>(), the linker can find them
template std::shared_ptr<Texture> ResourceManager::Get<Texture>(const std::string&);
template std::shared_ptr<AudioData> ResourceManager::Get<AudioData>(const std::string&);

// Loading function for textures
template<>
std::shared_ptr<Texture> ResourceManager::Load<Texture>(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    // If not found, return nullptr immediately (fail fast)
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    try {
        // Use the existing Texture constructor to load the file
        auto texture = std::make_shared<Texture>(filePath);

        // Check if texture loaded successfully
        if (texture->ID() == 0) {
            LOG_ERROR("Texture failed to load: " << filePath);
            return nullptr;
        }
        // Successful
        return texture;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading texture " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Loading function for audio
template<>
std::shared_ptr<AudioData> ResourceManager::Load<AudioData>(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    // Get and validate extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Just in case there's a leading dot
    if (!extension.empty() && extension[0] == '.') {
        extension = extension.substr(1);
    }

    // Validate format
    if (extension != "wav" && extension != "mp3" && extension != "ogg" &&
        extension != "flac" && extension != "m4a" && extension != "aac") {
        LOG_WARNING("Unsupported audio format: " << filePath);
        return nullptr;
    }

    // If file exists, then open in binary mode (because of audio files)
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR("Failed to open file: " << filePath);
        return nullptr;
    }

    // Find total file size
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Create audio object
    // make_shared is safer and won't leak memory
    auto audio = std::make_shared<AudioData>();
    audio->Path = filePath;
    audio->Format = extension;

    // Prepare vector for file data then read file into memory
    audio->Data.resize(fileSize);
    file.read(reinterpret_cast<char*>(audio->Data.data()), fileSize);

    // Checks
    if (!file) {
        LOG_ERROR("Failed to read file: " << filePath);
        return nullptr;
    }

    // Success
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

    // Try to remove from texture cache
    removed |= m_textures.erase(name) > 0;

    // Try to remove from audio cache
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
