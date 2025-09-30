#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <glad/glad.h>
#include "../include/graphics/vertex.hpp"
#include "../include/graphics/sprite.hpp"

class Renderer {
public:
    Renderer(size_t maxQuads = 3000);
    ~Renderer();

    void beginFrame();
    void endFrame();

    // Submit textured quad (sprite) to the batcher
    void submitQuad(const glm::vec2& pos,
        const glm::vec2& size,
        GLuint textureId,
        const glm::vec4& uvRect,   // (u0,v0,u1,v1)
        const glm::vec4& color,
        float rotation = 0.0f,
        float scale = 1.0f,
        int layer = 0);

    // Generic triangles for helpers (polygons/circles/etc.)
    void submitTriangles(const Vertex* verts, size_t vCount,
        const uint32_t* indices, size_t iCount,
        GLuint textureId,
        int layer = 0);

    void submitSprite(const Sprite& sprite);
    int flushCountThisFrame = 0;

private:
    // GL objects
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    // CPU-side batching
    std::vector<Vertex>   cpuBuffer;    // vertices
    std::vector<uint32_t> cpuIndices;   // indices

    // Current GPU buffer capacities (in elements)
    size_t vboCapacity = 0;
    size_t eboCapacity = 0;

    // Texture slot cache (for uTextures[N] shader array)
    static constexpr int MaxTextureSlots = 32;
    std::vector<GLuint> textureSlots;   // GL texture ids in slots 0..N-1

    // Helpers
    void ensureCapacity(size_t vNeeded, size_t iNeeded);
    void flush();
    void clearTextureSlots();
    int  getOrAssignTextureSlot(GLuint textureId, bool& flushed); // returns 0..N-1
    void bindTextureSlots() const;
};
