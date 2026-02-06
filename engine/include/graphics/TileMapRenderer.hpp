/* Start Header *****************************************************************/
/*!
\file   TileMapRenderer.hpp
\author Choi Meng Yew
\date   31st January 2026
\author Samantha Leong
\par    s.leong@digipen.edu
\date   3rd February 2026
\brief
Declares the TileMapRenderer, responsible for submitting tilemap geometry
to the renderer.

Details:
The TileMapRenderer translates TileMap and Tileset data into renderable
geometry and submits it to a rendering backend. It performs no rendering
itself and contains no state; it acts purely as a submission layer between
tile-based world data and the renderer.

This class does not own tile data, manage materials, or perform culling
or batching decisions beyond submission. It is intended to be used by
higher-level systems during scene rendering.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include "../include/core/World/TileMap.hpp"
#include "../include/core/World/TileSet.hpp"
#include <glm/vec2.hpp>
#include <vector>

class Renderer; // forward declare your batch renderer

// Submits tile geometry into the renderer
class TileMapRenderer
{
public:
    void Submit(
        const TileMap& tileMap,
        const std::vector<const Tileset*>& tilesets,
        Renderer& renderer,
        const glm::vec2& worldOffset
    ) const;
};
