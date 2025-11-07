/* Start Header *****************************************************************/
/*!
\file    Color.h
\authors Muhammad Nur Fadzly Bin Zulkifli (85%), Choi Meng Yew (15%)
\par     muhammadnurfadzly.b@digipen.edu, choimengyew.c@digipen.edu
\date    18th September 2025
\brief
Defines a Color struct for representing RGBA colors in both LDR and HDR formats.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef COLOR_H
#define COLOR_H

#include <algorithm>
#include <cstdint>
#include <glm/vec4.hpp>
#include <array>

using HexValue = uint32_t; // 0xRRGGBBAA format

struct Color {
    float R, G, B, A;  // Store as floats directly

    // Default constructor - LDR values
    explicit Color(HexValue red = 0, HexValue green = 0, HexValue blue = 0, HexValue alpha = 255)
        : R(red / 255.f), G(green / 255.f), B(blue / 255.f), A(alpha / 255.f) {
    }

    // HDR constructor - no clamping
    Color(float red, float green, float blue, float alpha = 1.f)
        : R(red), G(green), B(blue), A(alpha) {
    }

    // Convert to byte representation when needed for APIs that require it
    glm::vec4 ToVec4() const { return glm::vec4(R, G, B, A); }

    // Only clamp when converting to LDR byte format
    std::array<HexValue, 4> ToBytes() const {
        return {
            static_cast<HexValue>(std::clamp(R, 0.f, 1.f) * 255.f),
            static_cast<HexValue>(std::clamp(G, 0.f, 1.f) * 255.f),
            static_cast<HexValue>(std::clamp(B, 0.f, 1.f) * 255.f),
            static_cast<HexValue>(std::clamp(A, 0.f, 1.f) * 255.f)
        };
    }
};

#endif
