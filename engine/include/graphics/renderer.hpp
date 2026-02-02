/* Start Header *****************************************************************/
/*!
\file   renderer.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   24th September 2025
\brief
Declares the Renderer class, a low-level batching system responsible for
efficiently sending geometry to the GPU. It manages vertex/index buffers,
texture slots, and batched draw calls, automatically flushing when capacity
is exceeded.

Features:
- Batches quads, sprites, text, and generic triangles.
- Manages OpenGL buffer objects (VAO, VBO, EBO).
- Provides a texture slot cache for array-based texture binding.
- Designed to be called between beginFrame() and endFrame().
*/
/* End Header *******************************************************************/
#pragma once
#include "Export.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <glad/glad.h>
#include "font.hpp"
#include "graphics/vertex.hpp"
#include "graphics/sprite.hpp"

inline constexpr float kPixelsPerUnit = 100.0f; // 1 world unit == 100 pixels
inline float PixelsToUnits(float px) { return px / kPixelsPerUnit; }
inline float UnitsToPixels(float wu) { return wu * kPixelsPerUnit; }

class GRAPEENGINE_API Renderer {
public:
    Renderer(size_t maxQuads = 20000);
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
        int layer = 0,
        GLuint emissiveTextureId = 0,
        float emissiveStrength = 0.0f,
        GLuint normalTextureId = 0,
        GLuint mraTextureId = 0,
        float metallic = 0.0f,
        float smoothness = 0.5f,
        float aoStrength = 1.0f,
        float normalStrength = 1.0f,
        uint32_t materialFlags = 0);

    // Generic triangles for helpers (polygons/circles/etc.)
    void submitTriangles(const Vertex* verts, size_t vCount,
        const uint32_t* indices, size_t iCount,
        GLuint textureId,
        int layer = 0);

    void submitSprite(const Sprite& sprite);

    void submitText(const Font& font,
        std::string_view text,
        glm::vec2 pos,
        glm::vec4 color,
        float pixelSize);

    void drawFullscreenQuad() const;

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

    // ========================================================================
    // Texture Slot Management (Unified System)
    // ========================================================================

    // Texture slot allocation:
    // Slots 0-15:   Albedo/Diffuse textures    (MaxAlbedoSlots = 16)
    // Slots 16-23:  Emissive textures          (MaxEmissiveSlots = 8)
    // Slots 24-31:  Normal maps                (MaxNormalSlots = 8)
    // Slots 32-39:  MRA maps                   (MaxMRASlots = 8)
    // Total: 40 slots (well within GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)

    static constexpr int MaxAlbedoSlots = 16;
    static constexpr int MaxNormalSlots = 6;
    static constexpr int MaxMRASlots = 4;
    static constexpr int MaxEmissiveSlots = 6;

    static constexpr int AlbedoSlotBase = 0;
    static constexpr int NormalSlotBase = 16;
    static constexpr int MRASlotBase = 22;
    static constexpr int EmissiveSlotBase = 26;

    // Texture slot caches (store texture IDs currently bound)
    std::vector<GLuint> albedoTextureSlots;
    std::vector<GLuint> emissiveTextureSlots;
    std::vector<GLuint> normalTextureSlots;
    std::vector<GLuint> mraTextureSlots;

    // Helpers
    void ensureCapacity(size_t vNeeded, size_t iNeeded);
    void flush();
    void clearTextureSlots();

    // Unified texture slot assignment (returns local index 0..N-1)
    int getOrAssignTextureSlot(GLuint textureId, bool& flushed);
    int getOrAssignEmissiveTextureSlot(GLuint textureId, bool& flushed);
    int getOrAssignNormalTextureSlot(GLuint textureId, bool& flushed);
    int getOrAssignMRATextureSlot(GLuint textureId, bool& flushed);

    void bindTextureSlots() const;
};