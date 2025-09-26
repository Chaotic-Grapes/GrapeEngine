#include "ResourceManager.h"
#include <iostream>
#include <fstream>
#include <cmath>

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

    // If file exists, then open in binary mode (cause of image files)
    std::ifstream file(filePath, std::ios::binary);
    // Then usual check
    if (!file) {
        std::cout << "Could not open texture file: " << filePath << std::endl;
        return nullptr;
    }

    // Trying to find width
    file.seekg(0, std::ios::end);   // Move to end of file
    size_t fileSize = file.tellg(); // Get position (= file size)
    file.seekg(0, std::ios::beg);   // Move back to beginning

    // Placeholder texture
    std::shared_ptr<RTexture> texture = std::make_shared<RTexture>();
    texture->Path = filePath;
    texture->Width = static_cast<int>(sqrtf(fileSize));
    texture->Height = texture->Width;  // Assume image is square for now (I'll implement reading later)
    texture->Channels = 4;  // Assume RGBA for now
    texture->TextureID = 0; // No texture for now

    std::cout << "Read texture file: " << filePath << " (" << fileSize << " bytes)" << std::endl;
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
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cout << "Could not open audio file: " << filePath << std::endl;
        return nullptr;
    }

    // Find total file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // make_shared is safer and won't leak memory
    std::shared_ptr<RAudio> audio = std::make_shared<RAudio>();
    audio->Path = filePath;

    // Prepare vector for file data then read file into memory
    audio->Data.resize(fileSize);
    file.read(reinterpret_cast<char*>(audio->Data.data()), fileSize);

    // Placeholder audio
    audio->SampleRate = 44100;
    audio->Channels = 2;
    audio->BitsPerSample = 16;

    std::cout << "Read audio file: " << filePath << " (" << fileSize << " bytes)" << std::endl;
    return audio;
}
