#pragma once

#include <cstdint>
#include <limits>

// TileID is another name for the type uint16_t
using TileID = std::uint16_t;

// Sentinel value representing an empty / unassigned tile.
constexpr std::uint16_t EMPTY_TILE = 0xFFFF;

// Bitmask constants
constexpr std::uint16_t TILE_ID_MASK = 0x0FFF;      // Bottom 12 bits for ID (0-4095)
constexpr std::uint16_t TILE_ROT_MASK = 0xF000;     // Top 4 bits for Rotation
constexpr int TILE_ROT_SHIFT = 12;

// Helper to extract Base ID
inline TileID GetTileBaseID(TileID packed) { return packed & TILE_ID_MASK; }

// Helper to extract Rotation (0-15)
inline std::uint8_t GetTileRotation(TileID packed) { return (packed & TILE_ROT_MASK) >> TILE_ROT_SHIFT; }

// Helper to pack ID and Rotation
inline TileID PackTile(TileID id, std::uint8_t rotation) {
    return (id & TILE_ID_MASK) | ((static_cast<std::uint16_t>(rotation) << TILE_ROT_SHIFT) & TILE_ROT_MASK);
}
