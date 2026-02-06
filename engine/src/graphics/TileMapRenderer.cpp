/* Start Header *****************************************************************/
/*!
\file   TileMapRenderer.cpp
\author Choi Meng Yew (95%), Foo Rui Qin (5%)
\par    choi.m@digipen.edu, ruiqin.foo@digipen.edu

\brief
Definition of the TileMapRenderer class for rendering tilemaps. Provides
functionality to submit tilemap layers to the renderer using associated tilesets.
*/
/* End Header *******************************************************************/

#include "graphics/renderer.hpp"
#include "graphics/TileMapRenderer.hpp"
#include "core/World/TileLayer.hpp"

// Submit the tilemap contents to the renderer using the provided tilesets.
void TileMapRenderer::Submit(
    const TileMap& tileMap,
    const std::vector<const Tileset*>& tilesets,
    Renderer& renderer,
    const glm::vec2& worldOffset
) const
{
    const uint32_t layerIndex = 0; // render base layer for now
    const TileLayer& layer = tileMap.GetLayer(layerIndex);

    const uint32_t width = layer.Width();
    const uint32_t height = layer.Height();
    const float tileSize = tileMap.TileSize();

    const glm::vec4 color(1.0f); // white = no tint

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
                rotation,   // rotation
                1.0f,   // scale
                0       // render layer
            );
        }
    }
}
