/* Start Header *****************************************************************/
/*!
\file   TileMapRenderer.cpp
\author Choi Meng Yew (90%)
\author Samantha Leong (10%)
\par    s.leong@digipen.edu
\date   31st October 2025
\brief
Implementation of the TileMapRenderer, responsible for submitting tilemap
geometry to the renderer.

Details:
This file implements the logic that translates TileMap and Tileset data into
renderable quad submissions. It iterates over tile layers, resolves packed
TileID data (base ID, rotation, tileset index), computes world-space positions,
and submits textured quads with appropriate UVs and rotation.

The TileMapRenderer performs no rendering itself and maintains no persistent
state; it acts purely as a bridge between tile-based world data and the
rendering backend.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "graphics/renderer.hpp"
#include "graphics/TileMapRenderer.hpp"
#include "core/World/TileLayer.hpp"

void TileMapRenderer::Submit(
    const TileMap& tileMap,
    const std::vector<const Tileset*>& tilesets,
    Renderer& renderer,
    const glm::vec2& worldOffset,
    const ECS::Components::Material2D* material
) const
{
    const uint32_t layerIndex = 0; // render base layer for now
    const TileLayer& layer = tileMap.GetLayer(layerIndex);

    const uint32_t width = layer.Width();
    const uint32_t height = layer.Height();
    const float tileSize = tileMap.TileSize();

    const glm::vec4 color(1.0f); // white = no tint

    GLuint   normalTexId = material ? material->NormalTextureId : 0;
    GLuint   mraTexId = material ? material->MRA_TextureId : 0;
    float    metallic = material ? material->Metallic : 0.0f;
    float    smoothness = material ? material->Smoothness : 0.5f;
    float    aoStrength = material ? material->AOStrength : 1.0f;
    float    normalStrength = material ? material->NormalStrength : 1.0f;
    uint32_t matFlags = material ? material->Flags : 0;

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            TileID fullID = layer.Get(x, y);
            if (fullID == EMPTY_TILE)
                continue;

            TileID baseID = GetTileBaseID(fullID);
            uint8_t rotIdx = GetTileRotation(fullID);
            uint8_t tilesetIndex = GetTileTilesetIndex(fullID);

            if (tilesetIndex >= tilesets.size() || !tilesets[tilesetIndex]) {
                continue; // Skip tiles that reference missing tilesets.
            }

            const Tileset& tileset = *tilesets[tilesetIndex];
            const GLuint textureId = tileset.GetTextureId();

            TileUV uv;
            if (!tileset.GetTileUV(baseID, uv))
                continue;

            // Pack TileUV -> glm::vec4 (u0, v0, u1, v1)
            const glm::vec4 uvRect{
                uv.u0, uv.v0,
                uv.u1, uv.v1
            };

            const float half = tileSize * 0.5f; // Renderer::submitQuad expects center coordinates.
            const int32_t signedX = tileMap.OriginX() + static_cast<int32_t>(x);
            const int32_t signedY = tileMap.OriginY() + static_cast<int32_t>(y);

            const glm::vec2 pos{
                worldOffset.x + tileMap.TileToWorldSigned(signedX) + half,
                worldOffset.y + tileMap.TileToWorldSigned(signedY) + half
            }; // Convert signed tile coords to world space and add the tilemap's world origin.

            const glm::vec2 size{ tileSize, tileSize };
            
            // Rotation: 0=0, 1=90, 2=180, 3=270 (in radians)
            // 90 degrees = PI/2 = 1.57079632679f
            float rotation = static_cast<float>(rotIdx) * 1.57079632679f;

            renderer.submitQuad(
                pos,
                size,
                textureId,
                uvRect,
                color,
                rotation,
                1.0f,           // uniformScale
                0,              // layer
                0,              // emissiveTextureId
                0.0f,           // emissiveStrength
                normalTexId,
                mraTexId,
                metallic,
                smoothness,
                aoStrength,
                normalStrength,
                matFlags
            );
        }
    }
}
