#pragma once

namespace graphicsConfig {
    // ============================================================
    // WORLD SPACE CONFIGURATION
    // ============================================================

    /// Defines the relationship between world units and screen pixels
    /// 1.0 world unit = 100.0 pixels
    constexpr float PIXELS_PER_WORLD_UNIT = 100.0f; // Each world unit represents 100 pixels
    constexpr float WORLD_UNITS_PER_PIXEL = 0.01f;  // i.e. 1.0f / PIXELS_PER_WORLD_UNIT

    /// Helper: Convert pixels to world units
    inline constexpr float PixelsToWorld(float pixels) {
        return pixels * WORLD_UNITS_PER_PIXEL;
    }

    /// Helper: Convert world units to pixels
    inline constexpr float WorldToPixels(float worldUnits) {
        return worldUnits * PIXELS_PER_WORLD_UNIT;
    }
}