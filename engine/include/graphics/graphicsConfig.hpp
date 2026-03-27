/* Start Header *****************************************************************/
/*!
\file   graphicsConfig.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\brief
Defines world-space scaling constants and helper conversions between pixels
and world units used across renderer and editor-facing drawing code.

This header centralizes coordinate conversion policy so gameplay, tools,
and rendering subsystems interpret world scale consistently.
*/
/* End Header *******************************************************************/

#pragma once

namespace graphicsConfig {
    // ============================================================
    // WORLD SPACE CONFIGURATION
    // ============================================================

    /// Defines the relationship between world units and screen pixels
    /// 1.0 world unit = 100.0 pixels
    constexpr float PIXELS_PER_WORLD_UNIT = 100.0f; // Each world unit represents 100 pixels
    constexpr float WORLD_UNITS_PER_PIXEL = 0.01f;  // i.e. 1.0f / PIXELS_PER_WORLD_UNIT

    /**
     * @brief Convert a pixel value into world units.
     * @param pixels Pixel-space scalar value.
     * @return Equivalent world-space value.
     */
    inline constexpr float PixelsToWorld(float pixels) {
        return pixels * WORLD_UNITS_PER_PIXEL;
    }

    /**
     * @brief Convert a world-unit value into pixels.
     * @param worldUnits World-space scalar value.
     * @return Equivalent pixel-space value.
     */
    inline constexpr float WorldToPixels(float worldUnits) {
        return worldUnits * PIXELS_PER_WORLD_UNIT;
    }
}