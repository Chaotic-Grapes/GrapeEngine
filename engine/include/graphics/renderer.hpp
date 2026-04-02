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
#include "graphics/TextureFilter.h"

inline constexpr float kPixelsPerUnit = 100.0f; // 1 world unit == 100 pixels
inline float PixelsToUnits(float px) { return px / kPixelsPerUnit; }
inline float UnitsToPixels(float wu) { return wu * kPixelsPerUnit; }

class GRAPEENGINE_API Renderer {
public:
    /**
     * @brief Construct a batched renderer with fixed quad capacity.
     * @param maxQuads Maximum number of quads that can be batched before a flush is required.
     */
    Renderer(size_t maxQuads = 20000);

    /**
     * @brief Destroy renderer resources and release GPU objects.
     */
    ~Renderer();

    /**
     * @brief Begin a new CPU-side batch.
     */
    void beginFrame();

    /**
     * @brief Upload and draw the current batch if it contains geometry.
     */
    void endFrame();

    /**
     * @brief Submit one textured quad with optional emissive and material maps.
     * @param pos Quad center position in world space.
     * @param size Quad width/height in world units.
     * @param textureId Albedo texture id, or 0 for untextured.
     * @param uvRect UV rectangle as (u0, v0, u1, v1).
     * @param color Vertex tint color.
     * @param rotation Rotation in radians.
     * @param scale Uniform scale multiplier.
     * @param layer Reserved render layer value.
     * @param emissiveTextureId Emissive map texture id, or 0 to disable.
     * @param emissiveStrength Emissive multiplier used by the shader.
     * @param normalTextureId Normal map texture id, or 0 to disable.
     * @param mraTextureId Metallic-roughness-AO map texture id, or 0 to disable.
     * @param metallic Scalar metallic fallback value.
     * @param smoothness Scalar smoothness fallback value.
     * @param aoStrength Scalar ambient occlusion fallback value.
     * @param normalStrength Scalar normal influence fallback value.
     * @param materialFlags Bitfield interpreted by the material shader.
     * @param textureFilter Sampler filter used for albedo sampling.
     */
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
        uint32_t materialFlags = 0,
        Graphics::TextureFilter textureFilter = Graphics::TextureFilter::Nearest);

    /**
     * @brief Submit indexed triangle geometry to the current batch.
     * @param verts Pointer to vertex data.
     * @param vCount Number of vertices in verts.
     * @param indices Pointer to index data.
     * @param iCount Number of indices in indices.
     * @param textureId Albedo texture id to bind for this draw chunk, or 0.
     * @param layer Reserved render layer value.
     */
    void submitTriangles(const Vertex* verts, size_t vCount,
        const uint32_t* indices, size_t iCount,
        GLuint textureId,
        int layer = 0);

    /**
     * @brief Submit a sprite convenience struct as a quad draw.
     * @param sprite Sprite payload containing transform, UV, tint, and material settings.
     */
    void submitSprite(const Sprite& sprite);

    /**
     * @brief Submit text quads from a font atlas for the provided string.
     * @param font Font atlas and glyph metrics source.
     * @param text UTF-8 text to render.
     * @param pos Baseline anchor position.
     * @param color Text tint color.
     * @param pixelSize Requested glyph height in pixels.
     */
    void submitText(const Font& font,
        std::string_view text,
        glm::vec2 pos,
        glm::vec4 color,
        float pixelSize);

    /**
     * @brief Draw a full-screen quad using the currently bound pipeline state.
     */
    void drawFullscreenQuad() const;

    /** Number of flushes performed in the active frame. */
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
    struct AlbedoSlot {
        GLuint id = 0;
        Graphics::TextureFilter filter = Graphics::TextureFilter::Nearest;
    };

    std::vector<AlbedoSlot> albedoTextureSlots;
    std::vector<GLuint> emissiveTextureSlots;
    std::vector<GLuint> normalTextureSlots;
    std::vector<GLuint> mraTextureSlots;

    // Helpers
    void ensureCapacity(size_t vNeeded, size_t iNeeded);
    void flush();
    void clearTextureSlots();

    // Unified texture slot assignment (returns local index 0..N-1)
    int getOrAssignTextureSlot(GLuint textureId, Graphics::TextureFilter filter, bool& flushed);
    int getOrAssignEmissiveTextureSlot(GLuint textureId, bool& flushed);
    int getOrAssignNormalTextureSlot(GLuint textureId, bool& flushed);
    int getOrAssignMRATextureSlot(GLuint textureId, bool& flushed);

    void bindTextureSlots() const;

    GLuint m_samplerNearest = 0;
    GLuint m_samplerLinear = 0;
};
