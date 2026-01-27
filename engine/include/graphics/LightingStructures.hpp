#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Graphics {

    // ----------------------------------------------------------------------------
    // GPU-facing light layouts (keep POD / memcpy-friendly for SIMD optimization)
    // ----------------------------------------------------------------------------

    // Point light for forward shading (std140-friendly if used in UBO; also fine for SSBO).
    // PositionAndRange: xyz = world position, w = range
    // ColorAndIntensity: rgb = color (0-1), a = intensity multiplier
    struct GPUPointLight {
        glm::vec4 PositionAndRange{ 0.0f };                     // (x,y,z,range)
        glm::vec4 ColorAndIntensity{ 1.0f, 1.0f, 1.0f, 1.0f };  // (r,g,b,intensity)
    };

    // Directional light for forward shading.
    // Direction: xyz = direction (should be normalized), w unused
    // ColorAndIntensity: rgb = color (0-1), a = intensity multiplier
    struct GPUDirectionalLight {
        glm::vec4 Direction{ 0.0f, -1.0f, 0.0f, 0.0f };        // default: pointing down
        glm::vec4 ColorAndIntensity{ 1.0f, 1.0f, 1.0f, 1.0f }; // (r,g,b,intensity)
    };

    // ----------------------------------------------------------------------------
    // Global lighting limits (algorithm-independent)
    // ----------------------------------------------------------------------------
    // These are scene-level limits for buffer allocation, NOT Forward+ tile limits.
    namespace LightingConfig {
        constexpr uint32_t MAX_POINT_LIGHTS = 256;  // adjust as needed
        constexpr uint32_t MAX_DIR_LIGHTS = 1;      // usually 1 "sun/moon" in 2D?
    }

} // namespace Graphics
