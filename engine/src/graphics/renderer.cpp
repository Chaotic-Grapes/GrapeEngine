/* Start Header *****************************************************************/
/*!
\file   renderer.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   24th September 2025
\brief
This Renderer class is a low-level batching system responsible for preparing
geometry and sending it to the GPU efficiently. It manages vertex/index
buffers, texture slots, and batched draw calls.

Responsibilities:
- Store vertices/indices in CPU buffers until flushed.
- Manage GPU buffer objects (VAO, VBO, EBO).
- Handle texture slot assignment and binding.
- Provide APIs to submit quads, sprites, or raw triangles.
- Flush batches automatically when capacity is exceeded.

Not Responsible For:
- Window/context creation or buffer swapping.
- Clearing the screen or setting projection matrices.
- Managing shaders or camera transforms.
- High-level scene or ECS logic.

Intended Usage:
- Called by higher-level render systems 
- Should always be used between beginFrame() and endFrame().

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include "graphics/renderer.hpp"
#include "graphics/vertex.hpp"
#include "graphics/font.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>

// No need to grow since there is batch flushing when capacity is exceeded
void Renderer::ensureCapacity(size_t vNeeded, size_t iNeeded) {
    if (vNeeded > vboCapacity || iNeeded > eboCapacity) {
        throw std::runtime_error("Renderer: exceeded batch capacity. Did you forget to flush?");
    }
}

void Renderer::clearTextureSlots() {
    albedoTextureSlots.clear();
    emissiveTextureSlots.clear();
}

void Renderer::flush() {
    flushCountThisFrame++;

#if 0
    std::cout << "FLUSH: verts=" << cpuBuffer.size()
              << "/" << vboCapacity
              << " indices=" << cpuIndices.size()
              << "/" << eboCapacity
              << " texSlots=" << textureSlots.size() << std::endl;
#endif

    endFrame();
    beginFrame();
}

int Renderer::getOrAssignTextureSlot(GLuint textureId, bool& flushed) {
    flushed = false;

    // Check if texture is already bound
    for (int i = 0; i < (int)albedoTextureSlots.size(); ++i) {
        if (albedoTextureSlots[i] == textureId) {
            return i;
        }
    }

    // Check if we have room for a new texture
    if ((int)albedoTextureSlots.size() >= MaxAlbedoTextureSlots) {
        flushed = true;
        return -1; // Signal to caller: need to flush before retry
    }

    // Add new texture to available slot
    albedoTextureSlots.push_back(textureId);
    return (int)albedoTextureSlots.size() - 1;
}

int Renderer::getOrAssignEmissiveTextureSlot(GLuint textureId, bool& flushed) {
    flushed = false;

    // Check if texture is already bound
    for (int i = 0; i < (int)emissiveTextureSlots.size(); ++i) {
        if (emissiveTextureSlots[i] == textureId) {
            return i;
        }
    }

    // Check if we have room for a new texture
    if ((int)emissiveTextureSlots.size() >= MaxEmissiveTextureSlots) {
        flushed = true;
        return -1; // Signal to caller: need to flush before retry
    }

    // Add new texture to available slot
    emissiveTextureSlots.push_back(textureId);
    return (int)emissiveTextureSlots.size() - 1;
}

void Renderer::bindTextureSlots() const {
    // Bind albedo textures to slots 0-23
    for (int i = 0; i < (int)albedoTextureSlots.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, albedoTextureSlots[i]);
    }

    // Bind emissive textures to slots 24-31
    for (int i = 0; i < (int)emissiveTextureSlots.size(); ++i) {
        glActiveTexture(GL_TEXTURE24 + i);  // Start at slot 24
        glBindTexture(GL_TEXTURE_2D, emissiveTextureSlots[i]);
    }
}

Renderer::Renderer(size_t maxQuads) {
    vboCapacity = maxQuads * 4;
    eboCapacity = maxQuads * 6;
    cpuBuffer.reserve(vboCapacity);
    cpuIndices.reserve(eboCapacity);
    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glCreateBuffers(1, &ebo);
    glNamedBufferData(vbo, vboCapacity * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(ebo, eboCapacity * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(vao, ebo);

    // attributes
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoord));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribFormat(vao, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, color));
    glVertexArrayAttribBinding(vao, 2, 0);

    glEnableVertexArrayAttrib(vao, 3);
    glVertexArrayAttribFormat(vao, 3, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, texIndex));
    glVertexArrayAttribBinding(vao, 3, 0);

    // stroke thickness per vertex
    glEnableVertexArrayAttrib(vao, 4);
    glVertexArrayAttribFormat(vao, 4, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, strokePx));
    glVertexArrayAttribBinding(vao, 4, 0);

    // emissive texture index
    glEnableVertexArrayAttrib(vao, 5);
    glVertexArrayAttribFormat(vao, 5, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, emissiveTexIndex));
    glVertexArrayAttribBinding(vao, 5, 0);

    // emissive strength
    glEnableVertexArrayAttrib(vao, 6);
    glVertexArrayAttribFormat(vao, 6, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, emissiveStrength));
    glVertexArrayAttribBinding(vao, 6, 0);
}

Renderer::~Renderer() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void Renderer::beginFrame() {
    cpuBuffer.clear();
    cpuIndices.clear();
    clearTextureSlots(); // reset slot cache per frame
}

void Renderer::endFrame() {
    if (cpuBuffer.empty() || cpuIndices.empty()) return;

    glNamedBufferSubData(vbo, 0, cpuBuffer.size() * sizeof(Vertex), cpuBuffer.data());
    glNamedBufferSubData(ebo, 0, cpuIndices.size() * sizeof(uint32_t), cpuIndices.data());

    bindTextureSlots();

#if 0
    std::cout   << "[PROFILING]: verts=" << cpuBuffer.size()
                << "/" << vboCapacity
                << " indices=" << cpuIndices.size()
                << "/" << eboCapacity
                << " texSlots=" << textureSlots.size() << std::endl;
#endif

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)cpuIndices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------- public API ----------------
void Renderer::submitQuad(const glm::vec2& pos,
    const glm::vec2& size,
    GLuint textureId,
    const glm::vec4& uvRect,
    const glm::vec4& color,
    float rotation,
    float uniformScale,
    int /*layer*/,
    GLuint emissiveTextureId,      // NEW
    float emissiveStrength)     // NEW
    // Depth buffer kinda breaks transparency (Render everything back to front if u have transparent stuff)
{
    // Handle albedo texture
    float texIndex = -1.0f;
    if (textureId != 0) {
        bool flushed = false;
        int slot = getOrAssignTextureSlot(textureId, flushed);
        if (flushed) {
            flush();
            slot = getOrAssignTextureSlot(textureId, flushed);
        }
        texIndex = static_cast<float>(slot);
    }

    // Handle emissive texture
    float emissiveTexIndex = -1.0f;
    if (emissiveTextureId != 0 && emissiveStrength > 0.0f) {
        bool flushed = false;
        int slot = getOrAssignEmissiveTextureSlot(emissiveTextureId, flushed);
        if (flushed) {
            flush();
            slot = getOrAssignEmissiveTextureSlot(emissiveTextureId, flushed);
        }
        emissiveTexIndex = static_cast<float>(slot);
    }

    // Check buffer capacity
    if (cpuBuffer.size() + 4 > vboCapacity || cpuIndices.size() + 6 > eboCapacity) {
        flush();
    }

    // Calculate quad positions
    glm::vec2 half = size * 0.5f * uniformScale;
    glm::vec2 positions[4];
    if (rotation == 0.0f) {
        // Fast path for non-rotated quads
        positions[0] = { pos.x - half.x, pos.y - half.y }; // BL
        positions[1] = { pos.x + half.x, pos.y - half.y }; // BR
        positions[2] = { pos.x + half.x, pos.y + half.y }; // TR
        positions[3] = { pos.x - half.x, pos.y + half.y }; // TL
    }
    else {
        // Only calculate sin/cos once per sprite
        float c = cosf(rotation);
        float s = sinf(rotation);
        positions[0] = { pos.x + (-half.x * c + half.y * s), pos.y + (-half.x * s - half.y * c) };
        positions[1] = { pos.x + (half.x * c + half.y * s), pos.y + (half.x * s - half.y * c) };
        positions[2] = { pos.x + (half.x * c - half.y * s), pos.y + (half.x * s + half.y * c) };
        positions[3] = { pos.x + (-half.x * c - half.y * s), pos.y + (-half.x * s + half.y * c) };
    }

    // UV coordinates
    glm::vec2 uvs[4] = {
        {uvRect.x, uvRect.y}, {uvRect.z, uvRect.y},
        {uvRect.z, uvRect.w}, {uvRect.x, uvRect.w}
    };

    // Build vertices with emissive data
    size_t base = cpuBuffer.size();
    for (int i = 0; i < 4; ++i)
    {
        glm::vec3 pos3(positions[i], 0.0f); // promote 2D => 3D
        cpuBuffer.push_back({
            pos3,
            uvs[i],
            color,
            texIndex,
            0.0f,
            emissiveTexIndex,
            emissiveStrength
            });
    }

    // Indices
    cpuIndices.insert(cpuIndices.end(), {
        (uint32_t)base, (uint32_t)base + 1, (uint32_t)base + 2,
        (uint32_t)base + 2, (uint32_t)base + 3, (uint32_t)base
        });
}

void Renderer::submitTriangles(const Vertex* verts, size_t vCount,
    const uint32_t* indices, size_t iCount,
    GLuint textureId,
    int /*layer*/)
{
    float texIndex = -1.0f; // default to no texture

    if (textureId != 0) {
        bool flushed = false;
        int slot = getOrAssignTextureSlot(textureId, flushed);

        if (flushed) {
            flush();
            slot = getOrAssignTextureSlot(textureId, flushed);
        }

        texIndex = static_cast<float>(slot);
    }

    // Prevent overflow: if adding this batch (vCount verts, iCount indices) 
    // would exceed capacity, flush the current batch first.
    if (cpuBuffer.size() + vCount > vboCapacity ||
        cpuIndices.size() + iCount > eboCapacity) {
        flush();
    }

    size_t base = cpuBuffer.size();
    ensureCapacity(base + vCount, cpuIndices.size() + iCount);

    for (size_t i = 0; i < vCount; ++i) {
        Vertex v = verts[i];
        v.texIndex = texIndex;
        cpuBuffer.push_back(v);
    }

    for (size_t i = 0; i < iCount; ++i)
        cpuIndices.push_back((uint32_t)base + indices[i]);
}

void Renderer::submitSprite(const Sprite& sprite) {
    glm::vec2 adjustedSize = sprite.size;

    // Adjust size to maintain texture aspect ratio
    if (sprite.textureWidth > 0 && sprite.textureHeight > 0) {
        float aspectRatio = (float)sprite.textureWidth / (float)sprite.textureHeight;
        // Apply aspect ratio while preserving both scale.X and scale.Y
        adjustedSize.x = sprite.size.x * aspectRatio;
        adjustedSize.y = sprite.size.y;
    }

    submitQuad(sprite.pos, adjustedSize, sprite.textureId,
        sprite.uv, sprite.color,
        sprite.rotation, sprite.uniformScale,
        0,  // layer (default)
        sprite.emissiveTextureId,
        sprite.emissiveStrength);
}

void Renderer::submitText(const Font& font,
    std::string_view text,
    glm::vec2 pos,
    glm::vec4 color,
    float pixelSize)
{
    float scale = pixelSize / font.getPixelSize();
    float lineHeight = pixelSize * 1.2f; // we set a simple line height based on pixel size

    float startX = pos.x;
    float x = startX;
    float y = pos.y;

    for (char c : text) {
        // Handle newline
        if (c == '\n') {
            x = startX;
            y -= lineHeight; // Move DOWN (subtract because Y increases upwards in OpenGL)
            continue;
        }

        // Skip leading spaces on new lines
        if (x == startX && c == ' ') {
            continue;
        }

        const Glyph& g = font.getGlyph(c);

        // Only render if glyph has visible geometry
        if (g.size.x > 0 && g.size.y > 0) {
            float xpos = x + g.bearing.x * scale;
            float ypos = y - (g.size.y - g.bearing.y) * scale;

            float w = g.size.x * scale;
            float h = g.size.y * scale;

            glm::vec2 glyphPos = { xpos + w * 0.5f, ypos + h * 0.5f };
            glm::vec2 glyphSize = { w, h };

            submitQuad(glyphPos, glyphSize,
                font.getAtlasTexture(),
                g.uv,
                color,
                0.0f, 1.0f, 0);
        }
        // Apply global tracking adjustment
        float tracking = 1.05f;          // 5% extra spacing
        // float tracking = 1.0f;        // normal spacing
        
        // Always advance cursor (even for space)
        x += g.advance * scale * tracking;
    }
}

void Renderer::drawFullscreenQuad() const
{
    static GLuint fsVAO = 0, fsVBO = 0;
    if (fsVAO == 0)
    {
        // positions + texcoords (NDC)
        float quadVertices[] = {
            // pos      // uv
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &fsVAO);
        glGenBuffers(1, &fsVBO);
        glBindVertexArray(fsVAO);
        glBindBuffer(GL_ARRAY_BUFFER, fsVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    glBindVertexArray(fsVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}
