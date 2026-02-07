/* Start Header *****************************************************************/
/*!
\file   TileTypes.hpp
\author Choi Meng Yew
\date   31st January 2026
\author Samantha Leong
\par    s.leong@digipen.edu
\date   3rd February 2026
\brief
Defines the TileID type and helper utilities for packing and unpacking tile data.

Details:
This file specifies the TileID bit layout used throughout the tile system,
including base tile identifier, rotation, and tileset index packed into a
single 32-bit value. It also defines a sentinel value for empty tiles and
provides inline helpers for extracting and composing packed tile data.

These definitions are intentionally lightweight and header-only, serving as
a common foundation for tile layers, tilemaps, tilesets, and editor tooling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include <cstdint>
#include <limits>

// TileID stores packed tile data (base id, rotation, tileset index).
using TileID = std::uint32_t;

// Sentinel value representing an empty / unassigned tile.
constexpr std::uint32_t EMPTY_TILE = 0xFFFFFFFF;

// Bitmask constants
constexpr std::uint32_t TILE_ID_MASK = 0x00000FFF;      // Bottom 12 bits for ID (0-4095)
constexpr std::uint32_t TILE_ROT_MASK = 0x0000F000;     // Next 4 bits for Rotation
constexpr std::uint32_t TILESET_MASK = 0x00FF0000;      // Next 8 bits for Tileset index
constexpr int TILE_ROT_SHIFT = 12;
constexpr int TILESET_SHIFT = 16;

// Helper to extract Base ID
inline TileID GetTileBaseID(TileID packed) { return packed & TILE_ID_MASK; }

// Helper to extract Rotation (0-15)
inline std::uint8_t GetTileRotation(TileID packed) { return static_cast<std::uint8_t>((packed & TILE_ROT_MASK) >> TILE_ROT_SHIFT); }

// Helper to extract Tileset index (0-255)
inline std::uint8_t GetTileTilesetIndex(TileID packed) { return static_cast<std::uint8_t>((packed & TILESET_MASK) >> TILESET_SHIFT); }

// Helper to pack ID and Rotation
inline TileID PackTile(TileID id, std::uint8_t rotation, std::uint8_t tilesetIndex) {
    return (id & TILE_ID_MASK) |
        ((static_cast<TileID>(rotation) << TILE_ROT_SHIFT) & TILE_ROT_MASK) |
        ((static_cast<TileID>(tilesetIndex) << TILESET_SHIFT) & TILESET_MASK);
}
