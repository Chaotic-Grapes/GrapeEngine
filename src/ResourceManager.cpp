#include "ResourceManager.h"
#include <iostream>
#include <fstream>

// Add compile-time debug control
#ifdef _DEBUG
#define RM_DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define RM_DEBUG_PRINT(x) // No output in release
#endif

// Template specialization for Texture
template <>
std::shared_ptr<Texture> ResourceManager::Get<Texture>(const std::string& name) {
    // Check cache first
    std::unordered_map<std::string, std::shared_ptr<Texture>>::iterator it = m_textures.find(name);
    if (it != m_textures.end()) {
        RM_DEBUG_PRINT("[CACHE HIT] Texture: " << name);
        return it->second;  // Cache hit
    }

    // Not in cache: try to load
    RM_DEBUG_PRINT("[CACHE MISS] Loading texture: " << name);
    std::shared_ptr<Texture> texture = _loadTexture(name);
    if (texture && texture->ID() != 0) {
        m_textures[name] = texture;
        RM_DEBUG_PRINT("Cached texture: " << name
            << " | TexID = " << texture->ID()
            << " | Size = " << texture->Width() << "x" << texture->Height());
    }
    else {
        RM_DEBUG_PRINT("Failed to load texture: " << name);
    }
    return texture;
}

// Template specialization for RAudio
template <>
std::shared_ptr<AudioData> ResourceManager::Get<AudioData>(const std::string& name) {
    // Check cache first
    std::unordered_map<std::string, std::shared_ptr<AudioData>>::iterator it = m_audioFiles.find(name);
    if (it != m_audioFiles.end()) {
        RM_DEBUG_PRINT("[CACHE HIT] Audio: " << name);
        return it->second;  // Cache hit
    }

    // Not in cache: try to load
    RM_DEBUG_PRINT("[CACHE MISS] Loading audio: " << name);
    std::shared_ptr<AudioData> audio = _loadAudio(name);
    if (audio && audio->IsValid) {
        m_audioFiles[name] = audio;
        RM_DEBUG_PRINT("Cached audio: " << name
            << " | " << audio->Format << " format"
            << " | " << audio->Data.size() << " bytes");
    }
    else {
        RM_DEBUG_PRINT("Failed to load audio: " << name);
    }
    return audio;
}

// Loading function for textures
std::shared_ptr<Texture> ResourceManager::_loadTexture(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    // If not found, return nullptr immediately (fail fast)
    if (!std::filesystem::exists(filePath)) {
        RM_DEBUG_PRINT("File not found: " << filePath);
        return nullptr;
    }
    try {
        // Use the existing Texture constructor to load the file
        std::shared_ptr<Texture> texture = std::make_shared<Texture>(filePath);

        // Check if texture loaded successfully
        if (texture->ID() == 0) {
            RM_DEBUG_PRINT("Texture failed to load: " << filePath);
            return nullptr;
        }
        // Successful
        return texture;
    }
    catch (const std::exception& e) {
        RM_DEBUG_PRINT("Exception loading texture " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Loading function for audio
std::shared_ptr<AudioData> ResourceManager::_loadAudio(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    if (!std::filesystem::exists(filePath)) {
        RM_DEBUG_PRINT("File not found: " << filePath);
        return nullptr;
    }

    // Get and validate extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Just in case there's a leading dot
    if (!extension.empty() && extension[0] == '.') {
        extension = extension.substr(1);
    }

    // Validate format
    if (extension != "wav" && extension != "mp3" && extension != "ogg" &&
        extension != "flac" && extension != "m4a" && extension != "aac") {
        RM_DEBUG_PRINT("Unsupported audio format: " << filePath);
        return nullptr;
    }

    // If file exists, then open in binary mode (cause of audio files)
    // make_shared is safer and won't leak memory
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        RM_DEBUG_PRINT("Failed to open file: " << filePath);
        return nullptr;
    }

    // Find total file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Create audio object
    std::shared_ptr<AudioData> audio = std::make_shared<AudioData>();
    audio->Path = filePath;
    audio->Format = extension;

    // Prepare vector for file data then read file into memory
    audio->Data.resize(fileSize);
    file.read(reinterpret_cast<char*>(audio->Data.data()), fileSize);

    // Checks
    if (!file) {
        RM_DEBUG_PRINT("Failed to read file: " << filePath);
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
    RM_DEBUG_PRINT("Cleared all cached assets");
}

// Remove a specific asset from cache by name
void ResourceManager::UnloadAsset(const std::string& name) {
    bool removed = false;

    // Try to remove from texture cache
    if (m_textures.erase(name) > 0) {
        RM_DEBUG_PRINT("Unloaded texture: " << name);
        removed = true;
    }

    // Try to remove from audio cache
    if (m_audioFiles.erase(name) > 0) {
        RM_DEBUG_PRINT("Unloaded audio: " << name);
        removed = true;
    }

    if (!removed) {
        RM_DEBUG_PRINT("Asset not found in cache: " << name);
    }
}

// Get total number of cached assets
size_t ResourceManager::GetCacheSize() const {
    return m_textures.size() + m_audioFiles.size();
}

// Get cache info broken down by type
void ResourceManager::PrintCacheInfo() const {
    std::cout << "\nCache Info:" << std::endl;
    std::cout << "Textures: " << m_textures.size() << std::endl;
    std::cout << "Audio files: " << m_audioFiles.size() << std::endl;
    std::cout << "Total assets: " << GetCacheSize() << std::endl;
}

// Check if a specific asset is cached
bool ResourceManager::IsAssetCached(const std::string& name) const {
    return (m_textures.find(name) != m_textures.end()) ||
        (m_audioFiles.find(name) != m_audioFiles.end());
}
