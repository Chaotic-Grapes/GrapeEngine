#include "ResourceManager.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include "graphics/stb_image.h"

using RM = ResourceManager;

// Template specialization for RTexture
template <>
std::shared_ptr<RTexture> RM::Get<RTexture>(const std::string& name) {
    // Check cache first
    std::unordered_map<std::string, std::shared_ptr<RTexture>>::iterator it = m_textures.find(name);
    if (it != m_textures.end()) {
        std::cout << "Found texture in cache: " << name << std::endl;
        return it->second;
    }
    // Not in cache: try to load
    std::shared_ptr<RTexture> texture = _loadTexture(name);
    if (texture) {
        m_textures[name] = texture;
        std::cout << "Loaded texture from file and cached: " << name << std::endl;
    }
    else {
        std::cout << "Failed to load texture: " << name << std::endl;
    }
    return texture;
}

// Template specialization for RAudio
template <>
std::shared_ptr<RAudio> RM::Get<RAudio>(const std::string& name) {
    // Check cache first
    std::unordered_map<std::string, std::shared_ptr<RAudio>>::iterator it = m_audioFiles.find(name);
    if (it != m_audioFiles.end()) {
        std::cout << "Found audio in cache: " << name << std::endl;
        return it->second;
    }
    // Not in cache: try to load
    std::shared_ptr<RAudio> audio = _loadAudio(name);
    if (audio) {
        m_audioFiles[name] = audio;
        std::cout << "Loaded audio from file and cached: " << name << std::endl;
    }
    else {
        std::cout << "Failed to load audio: " << name << std::endl;
    }
    return audio;
}

// Loading function for textures
std::shared_ptr<RTexture> RM::_loadTexture(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    // If not found, print error message (the usual)
    if (!std::filesystem::exists(filePath)) {
        std::cout << "Texture file not found: " << filePath << std::endl;
        return nullptr;
    }

    // Use stb_image to load and decode the image
    int width, height, channels;
    unsigned char* imageData = stbi_load(filePath.c_str(), &width, &height, &channels, 0);

    // Checks
    if (!imageData) {
        std::cout << "Failed to decode image: " << filePath << " - " << stbi_failure_reason() << std::endl;
        return nullptr;
    }

    // Create texture object (with dimensions from imageData)
    std::shared_ptr<RTexture> texture = std::make_shared<RTexture>();
    texture->Path = filePath;
    texture->Width = width;     
    texture->Height = height;    
    texture->Channels = channels;
    texture->TextureID = 0;

    std::cout << "Loaded image: " << filePath << " (" << width << "x" << height << ", " 
        << channels << " channels)" << std::endl;

    // Free the image data since we're not storing pixels
    stbi_image_free(imageData);
    return texture;
}

// Loading function for audio
std::shared_ptr<RAudio> RM::_loadAudio(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    if (!std::filesystem::exists(filePath)) {
        std::cout << "Audio file not found: " << filePath << std::endl;
        return nullptr;
    }

    // If file exists, then open in binary mode (cause of audio files)
    // make_shared is safer and won't leak memory
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cout << "Could not open audio file: " << filePath << std::endl;
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
        std::cout << "File too small to be a valid WAV: " << filePath << std::endl;
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
        std::cout << "Invalid RIFF header in: " << filePath << std::endl;
        return nullptr;
    }

    // Validate WAVE format (same reason as above)
    if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E') {
        std::cout << "Invalid WAVE format in: " << filePath << std::endl;
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
        std::cout << "Invalid channel count (" << audio->Channels << ") in: " << filePath << std::endl;
        return nullptr;
    }

    // < 8k is super low-quality; > 192 beyond normal audio range
    if (audio->SampleRate < 8'000 || audio->SampleRate > 192'000) {
        std::cout << "Invalid sample rate (" << audio->SampleRate << ") in: " << filePath << std::endl;
        return nullptr;
    }

    // Success
    std::cout << "Loaded WAV: " << filePath << " (" << audio->SampleRate << "Hz, "
        << audio->Channels << " channels, " << audio->BitsPerSample << "-bit)" << std::endl;

    return audio;
}

// Clear all cached assets
void RM::ClearCache() {
    m_textures.clear();
    m_audioFiles.clear();
    std::cout << "Cleared all cached assets" << std::endl;
}

// Remove a specific asset from cache by name
void RM::UnloadAsset(const std::string& name) {
    bool removed = false;

    // Try to remove from texture cache
    if (m_textures.erase(name) > 0) {
        std::cout << "Unloaded texture: " << name << std::endl;
        removed = true;
    }

    // Try to remove from audio cache
    if (m_audioFiles.erase(name) > 0) {
        std::cout << "Unloaded audio: " << name << std::endl;
        removed = true;
    }

    if (!removed) {
        std::cout << "Asset not found in cache: " << name << std::endl;
    }
}

// Get total number of cached assets
size_t RM::GetCacheSize() const {
    return m_textures.size() + m_audioFiles.size();
}

// Get cache info broken down by type
void RM::PrintCacheInfo() const {
    std::cout << "\n\nCache Info:" << std::endl;
    std::cout << "Textures: " << m_textures.size() << std::endl;
    std::cout << "Audio files: " << m_audioFiles.size() << std::endl;
    std::cout << "Total assets: " << GetCacheSize() << std::endl;
}

// Check if a specific asset is cached
bool RM::IsAssetCached(const std::string& name) const {
    return (m_textures.find(name) != m_textures.end()) || 
        (m_audioFiles.find(name) != m_audioFiles.end());
}
