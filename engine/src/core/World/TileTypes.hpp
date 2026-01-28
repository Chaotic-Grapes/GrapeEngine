#pragma once

#include <cstdint>
#include <limits>

// TileID is another name for the type uint16_t
using TileID = std::uint16_t;

// Sentinel value representing an empty / unassigned tile.
constexpr std::uint16_t EMPTY_TILE = 0xFFFF;
