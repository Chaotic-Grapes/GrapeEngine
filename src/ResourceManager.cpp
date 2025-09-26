#include "ResourceManager.h"
#include <iostream>

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
    // Not in cache: create placeholder for now
    std::shared_ptr<RTexture> texture = std::make_shared<RTexture>();
    texture->Path = name;
    texture->Width = 256;    // Placeholder values
    texture->Height = 256;
    texture->Channels = 4;
    texture->TextureID = 1;  // Fake OpenGL ID for testing
    // Store in cache
    m_textures[name] = texture;
    std::cout << "Created placeholder texture and cached: " << name << std::endl;
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
    // Not in cache: create placeholder for now
    std::shared_ptr<RAudio> audio = std::make_shared<RAudio>();
    audio->Path = name;
    audio->SampleRate = 44100;  // Placeholder values
    audio->Channels = 2;
    audio->BitsPerSample = 16;
    // Store in cache
    m_audioFiles[name] = audio;
    std::cout << "Created placeholder audio and cached: " << name << std::endl;
    return audio;
}
