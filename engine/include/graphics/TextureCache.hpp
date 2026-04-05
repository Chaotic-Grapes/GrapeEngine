/* Start Header *****************************************************************/
/*!
\file   TextureCache.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Provides centralized management and caching of textures to avoid redundant GPU
uploads. When a texture is requested by path, it is loaded once and reused on
subsequent calls.
*/
/* End Header *******************************************************************/

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
    /**
     * @brief Load a texture from path, returning the cached instance on repeat requests.
     * @param path File path to the texture image.
     * @return Const reference to the cached Texture object.
     */
    static const Texture& Load(const std::string& path);

private:
    static std::unordered_map<std::string, Texture> cache;
};