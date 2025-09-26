#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <glad/glad.h>

// Asset structs
struct RTexture {
    GLuint TextureID = 0; // Default 0, invalid
    int Width = 0;
    int Height = 0;
    int Channels = 0;     // RGB = 3, RGBA = 4
    std::string Path;
    ~RTexture() {
        if (TextureID != 0)
            glDeleteTextures(1, &TextureID);
    }
};

struct RAudio {
    std::vector<uint8_t> Data;
    int SampleRate = 0;
    int Channels = 0;      // Mono = 1, stereo = 2, etc.
    int BitsPerSample = 0; // Quality: 8-bit, 16-bit, 24-bit, etc.
    std::string Path;
};

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // Main template function
    // Multiple objects use the same asset
    template <typename T>
    std::shared_ptr<T> Get(const std::string& name);

    // Template specializations (specific code for different assets)
    template <>
    std::shared_ptr<RTexture> Get<RTexture>(const std::string& name);
    template <>
    std::shared_ptr<RAudio> Get<RAudio>(const std::string& name);

private:
    // Asset caches (store loaded assets)
    std::unordered_map<std::string, std::shared_ptr<RTexture>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<RAudio>> m_audioFiles;
};

#endif