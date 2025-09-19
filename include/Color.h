#ifndef COLOR_H
#define COLOR_H

#include <algorithm>
#include <cstdint>

using HexValue = uint32_t; // 0xRRGGBBAA format

struct Color {
    HexValue R; // Red component (0-255)
    HexValue G; // Green component (0-255)
    HexValue B; // Blue component (0-255)
    HexValue A; // Alpha component (0-255), 0 is transparent, 255 is opaque

    explicit Color(const HexValue red = 0, const HexValue green = 0, const HexValue blue = 0, const HexValue alpha = 255)
        : R(red), G(green), B(blue), A(alpha) {
    }

    Color(const float red, const float green, const float blue, const float alpha = 1.f)
        : R(static_cast<HexValue>(std::clamp(red, 0.f, 1.f) * 255.f)),
          G(static_cast<HexValue>(std::clamp(green, 0.f, 1.f) * 255.f)),
          B(static_cast<HexValue>(std::clamp(blue, 0.f, 1.f) * 255.f)),
          A(static_cast<HexValue>(std::clamp(alpha, 0.f, 1.f) * 255.f)) {
    }
};

#endif
