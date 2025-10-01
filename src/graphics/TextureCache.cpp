/* Start Header *****************************************************************/
/*!
\file   TextureCache.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   27th September 2025
\brief
TextureCache ensures that each texture is only loaded once into GPU memory
and reused across multiple entities that reference the same file path.

Without this cache, every sprite that requests the same texture would
independently create a new OpenGL texture object. This leads to:
- Excessive GPU memory usage.
- Hundreds of duplicate texture IDs.
- Frequent batch flushes since the renderer can only bind a limited number
  of unique textures per draw call.

By centralizing texture ownership, the cache:
- Prevents redundant loads.
- Returns a consistent texture ID for the same file path.
- Greatly improves batching efficiency and performance.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include <iostream>
#include "../include/graphics/TextureCache.hpp"

std::unordered_map<std::string, Texture> TextureCache::cache;

const Texture& TextureCache::Load(const std::string& path) {
    auto it = cache.find(path);
    if (it != cache.end())
        return it->second;

    // Only log the first time we actually load it
    Texture tex(path);
    std::cout << "[TextureCache] Loaded " << path
        << " | TexId=" << tex.ID()
        << " | Size=" << tex.Width() << "x" << tex.Height()
        << std::endl;

    auto [inserted, _] = cache.emplace(path, std::move(tex));
    return inserted->second;
}