/* Start Header *****************************************************************/
/*!
\file   Color.h
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Defines the Color struct, a lightweight utility for representing RGBA color
values in both LDR (0-255) and HDR (floating-point) formats. It provides
convenient constructors, clamping, and conversion utilities for use across
the rendering pipeline.

Responsibilities:
- Store color values as normalized floats in linear space.
- Support initialization from both byte (0-255) and float (0.0-1.0) inputs.
- Provide conversions to glm::vec4 and clamped byte arrays for API compatibility.
- Ensure consistent color handling between CPU-side operations and GPU rendering.
*/
/* End Header *******************************************************************/

#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/vec4.hpp>
#include <array>

using HexValue = uint32_t; // 0xRRGGBBAA format

struct Color {
    float R, G, B, A;  // Store as floats directly

    // Default constructor - LDR values
    explicit Color(HexValue red = 0, HexValue green = 0, HexValue blue = 0, HexValue alpha = 255)
        : R(red   / 255.f), 
          G(green / 255.f), 
          B(blue  / 255.f), 
          A(alpha / 255.f) { /* EMPTY BY DESIGN */ }

    // HDR constructor - no clamping
    Color(float red, float green, float blue, float alpha = 1.f)
        : R(red), G(green), B(blue), A(alpha) { /* EMPTY BY DESIGN */ }

    /**
     * @brief Convert this color to a glm::vec4 for use in rendering APIs.
     * @return vec4 with (R, G, B, A) as float components.
     */
    glm::vec4 ToVec4() const { return glm::vec4(R, G, B, A); }

    /**
     * @brief Convert to LDR byte representation, clamping float values to [0, 1].
     * @return Array of four byte values {R, G, B, A} in the range [0, 255].
     */
    std::array<HexValue, 4> ToBytes() const {
        return {
            static_cast<HexValue>(std::clamp(R, 0.f, 1.f) * 255.f),
            static_cast<HexValue>(std::clamp(G, 0.f, 1.f) * 255.f),
            static_cast<HexValue>(std::clamp(B, 0.f, 1.f) * 255.f),
            static_cast<HexValue>(std::clamp(A, 0.f, 1.f) * 255.f)
        };
    }
};
