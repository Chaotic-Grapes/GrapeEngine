/* Start Header *****************************************************************/
/*!
\file   debugDraw2D.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Declares a lightweight 2D debug drawing API built on top of the Renderer. It
provides helper functions for drawing basic primitives such as circles, lines,
points, rectangles, and polygons. These utilities are primarily intended for
visual debugging (e.g., colliders, bounding boxes, paths) rather than
production rendering.

Functions:
- Circle: Draws a filled or stroked circle using a single quad and shader-based masking.
- Line: Renders a thick line as a quad.
- Point: Marks a position using a small square.
- RectStroke: Renders a rectangle outline with thick edges.
- RectFill: Renders a filled rectangle.
- Polygon: Renders a filled polygon (supports concave shapes).
*/
/* End Header *******************************************************************/

#pragma once
#include "Export.h"
#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>

class Renderer;

namespace DebugDraw2D {
    // Renders a filled circle when strokePx <= 0, or an outlined circle (of strokePx thickness in pixels) otherwise.
    GRAPEENGINE_API void Circle(Renderer& r,
        const glm::vec2& center,
        float radius,
        const glm::vec4& color,
        float strokePx,
        GLuint textureId = 0);

    // Thick line as a quad
    GRAPEENGINE_API void Line(Renderer& r,
        const glm::vec2& p1,
        const glm::vec2& p2,
        float thickness,
        const glm::vec4& color,
        GLuint textureId);

    // Square �point� (pixel marker)
    GRAPEENGINE_API void Point(Renderer& r,
        const glm::vec2& pos,
        float size,
        const glm::vec4& color,
        GLuint textureId);

    // Rectangle outline using 4 thick lines
    GRAPEENGINE_API void RectStroke(Renderer& r,
        const glm::vec2& min,
        const glm::vec2& max,
        float thickness,
        const glm::vec4& color,
        GLuint textureId);

    // Rectangle
    GRAPEENGINE_API void RectFill(Renderer& r,
        const glm::vec2& min,
        const glm::vec2& max,
        const glm::vec4& color,
        GLuint textureId);

    // Filled polygon (concave OK; simple, CCW preferred)
    GRAPEENGINE_API void Polygon(Renderer& r,
        const std::vector<glm::vec2>& points,
        const glm::vec4& color,
        GLuint textureId);

} // namespace DebugDraw2D
