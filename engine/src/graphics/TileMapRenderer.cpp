#include "graphics/renderer.hpp"
#include "graphics/TileMapRenderer.hpp"
#include "../src/core/World/TileLayer.hpp"

void TileMapRenderer::Submit(
    const TileMap& tileMap,
    const Tileset& tileset,
    Renderer& renderer
) const
{
    const uint32_t layerIndex = 0; // render base layer for now
    const TileLayer& layer = tileMap.GetLayer(layerIndex);

    const uint32_t width = layer.Width();
    const uint32_t height = layer.Height();
    const float tileSize = tileMap.TileSize();

    const GLuint textureId = tileset.GetTextureId();
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

            TileUV uv;
            if (!tileset.GetTileUV(baseID, uv))
                continue;

            // Pack TileUV -> glm::vec4 (u0, v0, u1, v1)
            const glm::vec4 uvRect{
                uv.u0, uv.v0,
                uv.u1, uv.v1
            };

            const glm::vec2 pos{
                tileMap.TileToWorld(x),
                tileMap.TileToWorld(y)
            };

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
