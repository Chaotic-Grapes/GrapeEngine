/* Start Header *****************************************************************/
/*!
\file   debugDraw2D.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Implements the DebugDraw2D namespace, providing helper functions for drawing
basic 2D primitives using the Renderer. These utilities are intended for
visual debugging, such as displaying colliders, bounding boxes, or guides
during development.

Functions:
- Circle: Renders a filled circle using triangle fan.
- Line: Renders a thick line as a quad between two points.
- Point: Marks a position with a square marker.
- RectStroke: Draws a rectangle outline with thick edges.
- RectFill: Draws a solid filled rectangle.
- Polygon: Renders a filled polygon (convex or concave) by triangulating
  the input vertices with the ear clipping algorithm.
*/
/* End Header *******************************************************************/

#include "graphics/debugDraw2D.hpp"
#include "graphics/renderer.hpp"
#include "graphics/vertex.hpp"
#include "graphics/polygon-utils.hpp"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstdint>

namespace {
    inline Vertex V(const glm::vec2& p, const glm::vec4& c, float strokePx = 0.0f) {
        Vertex v;
        v.position = glm::vec3(p, 0.0f); // promote 2D to 3D
        v.texCoord = { 0.f, 0.f }; // sample white
        v.color = c;
        v.texIndex = 0.f;        // Renderer overwrites with actual slot
        v.strokePx = strokePx;
        return v;
    }

    inline Vertex V(const glm::vec2& p, const glm::vec4& c, const glm::vec2& uv, float strokePx = 0.0f)
    {
        Vertex v;
        v.position = glm::vec3(p, 0.0f);
        v.texCoord = uv;            // allow explicit UVs
        v.color = c;
        v.texIndex = 0.f;
        v.strokePx = strokePx;
        return v;
    }

    inline void PushQuad(std::vector<Vertex>& outV, std::vector<uint32_t>& outI,
        const glm::vec2& a, const glm::vec2& b,
        const glm::vec2& c, const glm::vec2& d,
        const glm::vec4& color) {
        uint32_t base = static_cast<uint32_t>(outV.size());
        outV.push_back(V(a, color)); // 0
        outV.push_back(V(b, color)); // 1
        outV.push_back(V(c, color)); // 2
        outV.push_back(V(d, color)); // 3
        outI.push_back(base + 0); outI.push_back(base + 1); outI.push_back(base + 2);
        outI.push_back(base + 2); outI.push_back(base + 3); outI.push_back(base + 0);
    }
} // anon namespace

namespace DebugDraw2D {

    void Circle(Renderer& r,
        const glm::vec2& center,
        float radius,
        const glm::vec4& color,
        float strokePx,
        GLuint textureId)
    {
        // Slightly enlarge both geometry and UVs so the circle's edge (|vUV| = 1)
        // stays comfortably inside the quad (prevents clipping during anti-aliasing)
        const float pad = 1.10f; // 10% margin
        const glm::vec2 half{ radius * pad, radius * pad };

        // Define quad corners (positions + matching UVs)
        Vertex v0 = { glm::vec3(center + glm::vec2(-half.x, -half.y), 0.0f), {-pad, -pad}, color, 0.0f, strokePx };
        Vertex v1 = { glm::vec3(center + glm::vec2(half.x, -half.y), 0.0f),  { pad, -pad}, color, 0.0f, strokePx };
        Vertex v2 = { glm::vec3(center + glm::vec2(half.x,  half.y), 0.0f),  { pad,  pad}, color, 0.0f, strokePx };
        Vertex v3 = { glm::vec3(center + glm::vec2(-half.x,  half.y), 0.0f), {-pad,  pad}, color, 0.0f, strokePx };

        // 2 triangles forming the quad
        const uint32_t idx[] = { 0, 1, 2, 2, 3, 0 };
        const Vertex   vb[] = { v0, v1, v2, v3 };

        // Send to renderer - the fragment shader will compute the actual circular mask
        r.submitTriangles(vb, 4, idx, 6, textureId);
    }

    void Line(Renderer& r,
        const glm::vec2& p1,
        const glm::vec2& p2,
        float thickness,
        const glm::vec4& color,
        GLuint textureId)
    {
        glm::vec2 dir = p2 - p1; // edge vector
        float len = std::max(std::sqrt(dir.x * dir.x + dir.y * dir.y), 1e-6f);
        dir /= len;
        glm::vec2 n = { -dir.y, dir.x }; // outward normal
        glm::vec2 off = n * (thickness * 0.5f);

        glm::vec2 v0 = p1 - off; // BL
        glm::vec2 v1 = p1 + off; // TL
        glm::vec2 v2 = p2 + off; // TR
        glm::vec2 v3 = p2 - off; // BR

        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        verts.reserve(4); idx.reserve(6);
        PushQuad(verts, idx, v0, v1, v2, v3, color);

        r.submitTriangles(verts.data(), verts.size(), idx.data(), idx.size(), textureId);
    }

    void Point(Renderer& r,
        const glm::vec2& pos,
        float size,
        const glm::vec4& color,
        GLuint textureId)
    {
        glm::vec2 h = { size * 0.5f, size * 0.5f };
        glm::vec2 a = { pos.x - h.x, pos.y - h.y };
        glm::vec2 b = { pos.x - h.x, pos.y + h.y };
        glm::vec2 c = { pos.x + h.x, pos.y + h.y };
        glm::vec2 d = { pos.x + h.x, pos.y - h.y };

        std::vector<Vertex> verts; std::vector<uint32_t> idx;
        verts.reserve(4); idx.reserve(6);
        PushQuad(verts, idx, a, b, c, d, color);

        r.submitTriangles(verts.data(), verts.size(), idx.data(), idx.size(), textureId);
    }

    void RectStroke(Renderer& r,
        const glm::vec2& min,
        const glm::vec2& max,
        float thickness,
        const glm::vec4& color,
        GLuint textureId)
    {
        const float halfThick = thickness * 0.5f;

        // Outer rectangle (expanded by half-thickness)
        const glm::vec2 outerBL = min - glm::vec2(halfThick, halfThick);
        const glm::vec2 outerBR = { max.x + halfThick, min.y - halfThick };
        const glm::vec2 outerTR = max + glm::vec2(halfThick, halfThick);
        const glm::vec2 outerTL = { min.x - halfThick, max.y + halfThick };

        // Inner rectangle (shrunk by half-thickness)
        const glm::vec2 innerBL = min + glm::vec2(halfThick, halfThick);
        const glm::vec2 innerBR = { max.x - halfThick, min.y + halfThick };
        const glm::vec2 innerTR = max - glm::vec2(halfThick, halfThick);
        const glm::vec2 innerTL = { min.x + halfThick, max.y - halfThick };

        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;
        verts.reserve(16);
        idx.reserve(24);

        // Bottom edge (quad)
        PushQuad(verts, idx, outerBL, outerBR, innerBR, innerBL, color);

        // Right edge (quad)
        PushQuad(verts, idx, outerBR, outerTR, innerTR, innerBR, color);

        // Top edge (quad)
        PushQuad(verts, idx, outerTR, outerTL, innerTL, innerTR, color);

        // Left edge (quad)
        PushQuad(verts, idx, outerTL, outerBL, innerBL, innerTL, color);

        r.submitTriangles(verts.data(), verts.size(), idx.data(), idx.size(), textureId);
    }

    void RectFill(Renderer& r,
        const glm::vec2& min,
        const glm::vec2& max,
        const glm::vec4& color,
        GLuint textureId)
    {
        glm::vec2 v0 = { min.x, min.y }; // bottom-left
        glm::vec2 v1 = { max.x, min.y }; // bottom-right
        glm::vec2 v2 = { max.x, max.y }; // top-right
        glm::vec2 v3 = { min.x, max.y }; // top-left

        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;
        verts.reserve(4); idx.reserve(6);

        PushQuad(verts, idx, v0, v1, v2, v3, color);

        r.submitTriangles(verts.data(), verts.size(), idx.data(), idx.size(), textureId);
    }

    void Polygon(Renderer& r,
        const std::vector<glm::vec2>& points,
        const glm::vec4& color,
        GLuint textureId)
    {
        auto tris = triangulateEarClipping(points);
        if (tris.empty()) return;

        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;
        verts.reserve(tris.size() * 3);
        idx.reserve(tris.size() * 3);

        for (const auto& t : tris) {
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back(V(t.a, color));
            verts.push_back(V(t.b, color));
            verts.push_back(V(t.c, color));
            idx.push_back(base + 0);
            idx.push_back(base + 1);
            idx.push_back(base + 2);
        }

        r.submitTriangles(verts.data(), verts.size(), idx.data(), idx.size(), textureId);
    }

} // namespace DebugDraw2D