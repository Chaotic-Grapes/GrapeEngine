#include "../include/graphics/renderer.hpp"
#include "../include/graphics/sprite.hpp"
#include <glm/gtc/constants.hpp>
#include <iostream>

struct Vertex {
    glm::vec2 position;
    glm::vec2 texCoord;
    glm::vec4 color;
    float texIndex;
};

Renderer::Renderer(size_t maxQuads) {
    maxVertices = maxQuads * 6; // 6 vertices per quad
    cpuBuffer.reserve(maxVertices);

    // Create VAO and VBO
    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);

    glNamedBufferData(vbo, maxVertices * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

    // Link VBO to VAO
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));

    // Attribute layout
    glEnableVertexArrayAttrib(vao, 0); // pos
    glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1); // texCoord
    glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoord));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnableVertexArrayAttrib(vao, 2); // color
    glVertexArrayAttribFormat(vao, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, color));
    glVertexArrayAttribBinding(vao, 2, 0);

    glEnableVertexArrayAttrib(vao, 3); // texIndex
    glVertexArrayAttribFormat(vao, 3, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, texIndex));
    glVertexArrayAttribBinding(vao, 3, 0);
}

Renderer::~Renderer() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void Renderer::beginFrame() {
    cpuBuffer.clear();
}

void Renderer::submitQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color, float texIndex) {
    glm::vec2 half = size * 0.5f;

    glm::vec2 positions[4] = {
        {pos.x - half.x, pos.y - half.y},
        {pos.x + half.x, pos.y - half.y},
        {pos.x + half.x, pos.y + half.y},
        {pos.x - half.x, pos.y + half.y}
    };

    glm::vec2 uvs[4] = {
        {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}
    };

    // Two triangles (6 vertices)
    cpuBuffer.push_back({ positions[0], uvs[0], color, texIndex });
    cpuBuffer.push_back({ positions[1], uvs[1], color, texIndex });
    cpuBuffer.push_back({ positions[2], uvs[2], color, texIndex });

    cpuBuffer.push_back({ positions[2], uvs[2], color, texIndex });
    cpuBuffer.push_back({ positions[3], uvs[3], color, texIndex });
    cpuBuffer.push_back({ positions[0], uvs[0], color, texIndex });
}

void Renderer::submitCircle(const glm::vec2& center, float radius,
    const glm::vec4& color, int segments) {
    if (segments < 3) segments = 3; // minimum triangle

    float step = glm::two_pi<float>() / segments;
    glm::vec2 prev = { center.x + radius, center.y };

    for (int i = 1; i <= segments; i++) {
        float angle = i * step;
        glm::vec2 curr = { center.x + cos(angle) * radius,
                           center.y + sin(angle) * radius };

        cpuBuffer.push_back({ center, {0,0}, color, 0 });
        cpuBuffer.push_back({ prev,   {0,0}, color, 0 });
        cpuBuffer.push_back({ curr,   {0,0}, color, 0 });

        prev = curr;
    }
}

void Renderer::submitLine(const glm::vec2& p1, const glm::vec2& p2,
    const glm::vec4& color, float thickness) {
    glm::vec2 dir = glm::normalize(p2 - p1);
    glm::vec2 normal = { -dir.y, dir.x }; // perpendicular

    glm::vec2 offset = normal * (thickness * 0.5f);

    glm::vec2 v0 = p1 - offset;
    glm::vec2 v1 = p1 + offset;
    glm::vec2 v2 = p2 + offset;
    glm::vec2 v3 = p2 - offset;

    cpuBuffer.push_back({ v0, {0,0}, color, 0 });
    cpuBuffer.push_back({ v1, {0,0}, color, 0 });
    cpuBuffer.push_back({ v2, {0,0}, color, 0 });

    cpuBuffer.push_back({ v2, {0,0}, color, 0 });
    cpuBuffer.push_back({ v3, {0,0}, color, 0 });
    cpuBuffer.push_back({ v0, {0,0}, color, 0 });
}

void Renderer::submitPoint(const glm::vec2& pos,
    float size, const glm::vec4& color) {
    glm::vec2 half = { size * 0.5f, size * 0.5f };

    glm::vec2 v0 = { pos.x - half.x, pos.y - half.y };
    glm::vec2 v1 = { pos.x + half.x, pos.y - half.y };
    glm::vec2 v2 = { pos.x + half.x, pos.y + half.y };
    glm::vec2 v3 = { pos.x - half.x, pos.y + half.y };

    cpuBuffer.push_back({ v0, {0,0}, color, 0 });
    cpuBuffer.push_back({ v1, {0,0}, color, 0 });
    cpuBuffer.push_back({ v2, {0,0}, color, 0 });

    cpuBuffer.push_back({ v2, {0,0}, color, 0 });
    cpuBuffer.push_back({ v3, {0,0}, color, 0 });
    cpuBuffer.push_back({ v0, {0,0}, color, 0 });
}

void Renderer::submitRect(const glm::vec2& min,
    const glm::vec2& max,
    const glm::vec4& color,
    float thickness) {
    glm::vec2 v0 = { min.x, min.y };
    glm::vec2 v1 = { max.x, min.y };
    glm::vec2 v2 = { max.x, max.y };
    glm::vec2 v3 = { min.x, max.y };

    submitLine(v0, v1, color, thickness);
    submitLine(v1, v2, color, thickness);
    submitLine(v2, v3, color, thickness);
    submitLine(v3, v0, color, thickness);
}

inline void Renderer::submitSprite(const Sprite& sprite, const glm::vec4& color) {
    Renderer::submitQuad(sprite.pos, sprite.size, color, (float)sprite.texture);
}

void Renderer::endFrame() {
    if (cpuBuffer.empty()) return;

    // Upload to GPU
    glNamedBufferSubData(vbo, 0, cpuBuffer.size() * sizeof(Vertex), cpuBuffer.data());

    // Draw
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)cpuBuffer.size());
    glBindVertexArray(0);
}
