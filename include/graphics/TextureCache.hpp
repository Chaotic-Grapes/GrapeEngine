#pragma once

#include <string>
#include <unordered_map>
#include "../include/graphics/texture.hpp"

struct CachedTexture {
    GLuint id;
    int width;
    int height;
};

class TextureCache {
public:
    static const Texture& Load(const std::string& path);

private:
    static std::unordered_map<std::string, Texture> cache;
};