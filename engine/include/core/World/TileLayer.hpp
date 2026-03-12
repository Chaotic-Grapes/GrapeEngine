/* Start Header *****************************************************************/
/*!
\file   TileLayer.hpp
\author Choi Meng Yew (100%)
\date   31st January 2026
\brief
Defines a single tile layer represented as a fixed-size 2D grid of TileID values.

Details:
The TileLayer class encapsulates a row-major, linear storage of tiles with
explicit width and height. It provides safe read and write access with
bounds checking, returning EMPTY_TILE for out-of-range reads and ignoring
out-of-range writes.

This class is a lightweight data container with no rendering or engine logic
and is intended to be used by higher-level tilemap systems for world
representation and editing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include <vector>
#include <cstdint>
#include <cassert>

#include "TileTypes.hpp" // defines TileID and EMPTY_TILE

// Represents a single tile layer: a fixed-size 2D grid of TileID.
// Storage is linear and row-major.
class TileLayer
{
public:
    // Construct a layer with fixed width and height, initialized to EMPTY_TILE
    TileLayer(uint32_t width, uint32_t height)
        : m_width(width),
        m_height(height),
        m_tiles(width* height, EMPTY_TILE)
    {
        assert(width > 0 && height > 0);
    }

    // Safe read access
    // Returns EMPTY_TILE if out of bounds
    TileID Get(uint32_t x, uint32_t y) const
    {
        if (!InBounds(x, y))
            return EMPTY_TILE;

        return m_tiles[Index(x, y)];
    }

    // Safe write access
    // Ignores writes that are out of bounds
    void Set(uint32_t x, uint32_t y, TileID id)
    {
        if (!InBounds(x, y))
            return;

        m_tiles[Index(x, y)] = id;
    }

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

private:
    uint32_t m_width;
    uint32_t m_height;

    // Linear row-major storage: index = y * width + x
    std::vector<TileID> m_tiles;

    bool InBounds(uint32_t x, uint32_t y) const
    {
        return x < m_width && y < m_height;
    }

    uint32_t Index(uint32_t x, uint32_t y) const
    {
        return y * m_width + x;
    }
};
