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
std::shared_ptr<RAudio> ResourceManager::Get<RAudio>(const std::string& name) {
    // Check cache first
    std::unordered_map<std::string, std::shared_ptr<RAudio>>::iterator it = m_audioFiles.find(name);
    if (it != m_audioFiles.end()) {
        RM_DEBUG_PRINT("[CACHE HIT] Audio: " << name);
        return it->second;  // Cache hit
    }

    // Not in cache: try to load
    RM_DEBUG_PRINT("[CACHE MISS] Loading audio: " << name);
    std::shared_ptr<RAudio> audio = _loadAudio(name);
    if (audio && audio->IsValid) {
        m_audioFiles[name] = audio;
        RM_DEBUG_PRINT("Cached audio: " << name
            << " | " << audio->SampleRate << "Hz"
            << " | " << audio->Channels << " channels"
            << " | " << audio->BitsPerSample << "-bit");
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
std::shared_ptr<RAudio> ResourceManager::_loadAudio(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    if (!std::filesystem::exists(filePath)) {
        return nullptr;
    }

    // If file exists, then open in binary mode (cause of audio files)
    // make_shared is safer and won't leak memory
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return nullptr;
    }

    // Find total file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // WAV FILE STRUCTURE:
    // Bytes 0 - 3: RIFF header
    // Bytes 4 - 7: RIFF chunk size
    // Bytes 8 - 11: WAVE header
    // Bytes 12 - 15: FMT header
    // Bytes 16 - 19: Format chunk size
    // Bytes 20 - 21: Audio format (1 = PCM, 6 = mu-law, 7 = a-law)
    // Bytes 22 - 23: Number of channels (1 = mono, 2 = stereo)
    // Bytes 24 - 27: Sample rate (in Hz)
    // Bytes 28 - 31: Byte rate
    // Bytes 32 - 33: Block align (2 = 16-bit mono, 4 = 16-bit stereo)
    // Bytes 34 - 35: Bits per sample
    // Other stuff (up to byte 43, so that's total 44 bytes minimum)

    // Check minimum size for WAV header (44 bytes)
    if (fileSize < 44) {
        return nullptr;
    }

    // Create audio object
    std::shared_ptr<RAudio> audio = std::make_shared<RAudio>();
    audio->Path = filePath;

    // Prepare vector for file data then read file into memory
    audio->Data.resize(fileSize);
    file.read(reinterpret_cast<char*>(audio->Data.data()), fileSize);

    // Parse WAV header from the loaded data
    uint8_t* data = audio->Data.data();

    // Validate RIFF header (to check if it's actually a WAV file)
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') {
        return nullptr;
    }

    // Validate WAVE format (same reason as above)
    if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E') {
        return nullptr;
    }

    // Extract real values from header (little-endian)
    // Rn the numbers are split up and stored as separate bytes in the file

    // E.g. this means take value in slot 22, add it to slot 23 * 256 to get original number 
    // (<< 8 is just a fast way to multiply by 256 (2^8): base-256 system)
    audio->Channels = data[22] | (data[23] << 8);

    // Same concept but for a 4-byte number instead of 2-byte
    audio->SampleRate = data[24] | (data[25] << 8) | (data[26] << 16) | (data[27] << 24);
    audio->BitsPerSample = data[34] | (data[35] << 8);

    // Validate reasonable ranges
    // Number of audio channels < 1 (no audio channels); > 8 (weird)
    if (audio->Channels < 1 || audio->Channels > 8) {
        return nullptr;
    }

    // < 8k is super low-quality; > 192 beyond normal audio range
    if (audio->SampleRate < 8'000 || audio->SampleRate > 192'000) {
        return nullptr;
    }

    // Success
    RM_DEBUG_PRINT("Loaded WAV: " << filePath << " (" << audio->SampleRate << "Hz, "
        << audio->Channels << " channels, " << audio->BitsPerSample << "-bit)");

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
