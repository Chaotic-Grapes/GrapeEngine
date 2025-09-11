#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>
#include "../include/graphics/sprite.hpp"

struct Vertex;

class Renderer {
public:
    Renderer(size_t maxQuads = 1000);
    ~Renderer();

    void beginFrame();
    void endFrame();

    void submitQuad(const glm::vec2& pos,
        const glm::vec2& size,
        const glm::vec4& color,
        float texIndex = 0.0f);

    void submitCircle(const glm::vec2& center,
        float radius,
        const glm::vec4& color,
        int segments = 32);

    void submitLine(const glm::vec2& p1,
        const glm::vec2& p2,
        const glm::vec4& color,
        float thickness = 0.01f);

    void submitPoint(const glm::vec2& pos,
        float size,
        const glm::vec4& color);

    void submitRect(const glm::vec2& min,
        const glm::vec2& max,
        const glm::vec4& color,
        float thickness);

    void submitSprite(const Sprite& sprite, const glm::vec4& color);

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    size_t maxVertices;
    std::vector<Vertex> cpuBuffer;
};