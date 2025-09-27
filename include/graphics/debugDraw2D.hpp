#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>

class Renderer;

namespace DebugDraw2D {

    // Filled circle
    void Circle(Renderer& r,
        const glm::vec2& center,
        float radius,
        const glm::vec4& color,
        int segments,
        GLuint textureId);

    // Thick line as a quad
    void Line(Renderer& r,
        const glm::vec2& p1,
        const glm::vec2& p2,
        float thickness,
        const glm::vec4& color,
        GLuint textureId);

    // Square “point” (pixel marker)
    void Point(Renderer& r,
        const glm::vec2& pos,
        float size,
        const glm::vec4& color,
        GLuint textureId);

    // Rectangle outline using 4 thick lines
    void RectStroke(Renderer& r,
        const glm::vec2& min,
        const glm::vec2& max,
        float thickness,
        const glm::vec4& color,
        GLuint textureId);

    // Rectangle
    void RectFill(Renderer& r,
        const glm::vec2& min,
        const glm::vec2& max,
        const glm::vec4& color,
        GLuint textureId);

    // Filled polygon (concave OK; simple, CCW preferred)
    void Polygon(Renderer& r,
        const std::vector<glm::vec2>& points,
        const glm::vec4& color,
        GLuint textureId);

} // namespace DebugDraw2D
