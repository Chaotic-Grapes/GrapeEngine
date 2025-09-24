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

#include "../include/graphics/renderer.hpp"
#include "../include/graphics/vertex.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cassert>
#include <cstdint>

// No need to grow since there is batch flushing when capacity is exceeded
void Renderer::ensureCapacity(size_t vNeeded, size_t iNeeded) {
    assert(vNeeded <= vboCapacity && iNeeded <= eboCapacity &&
        "Renderer: exceeded batch capacity. Did you forget to flush?");
}

void Renderer::clearTextureSlots() { textureSlots.clear(); }

void Renderer::flush() {
    endFrame();
    beginFrame();
}

int Renderer::getOrAssignTextureSlot(GLuint textureId, bool& flushed) {
    flushed = false;

    for (int i = 0; i < (int)textureSlots.size(); ++i)
        if (textureSlots[i] == textureId) return i;

    if ((int)textureSlots.size() >= MaxTextureSlots) {
        flushed = true;
        return -1; // signal to caller: need to flush before retry (Magic number; change to enum)
    }

    textureSlots.push_back(textureId);
    return (int)textureSlots.size() - 1;
}

void Renderer::bindTextureSlots() const {
    for (int i = 0; i < (int)textureSlots.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textureSlots[i]);
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
    glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
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

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)cpuIndices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------- public API ----------------
void Renderer::drawQuad(const glm::vec2& pos,
    const glm::vec2& size,
    GLuint textureId,
    const glm::vec4& uvRect,
    const glm::vec4& color,
    float rotation,
    float uniformScale,
    int /*layer*/)
    // Depth buffer kinda breaks transparency (Render everything back to front if u have transparent stuff)
{
    float texIndex = -1.0f; // sentinel for "no texture"

    if (textureId != 0) {
        bool flushed = false;
        int slot = getOrAssignTextureSlot(textureId, flushed);

        if (flushed) {
            flush(); // finish current batch safely
            slot = getOrAssignTextureSlot(textureId, flushed); // retry in fresh batch
        }

        texIndex = static_cast<float>(slot);
    }

    glm::vec2 half = size * 0.5f;
    glm::vec2 local[4] = {
        {-half.x, -half.y}, // BL
        { half.x, -half.y}, // BR
        { half.x,  half.y}, // TR
        {-half.x,  half.y}  // TL
    };

    // build transform (translate * rotate * scale)
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, glm::vec3(pos, 0.0f));
    transform = glm::rotate(transform, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, glm::vec3(uniformScale, uniformScale, 1.0f));

    glm::vec2 uvs[4] = {
        {uvRect.x, uvRect.y}, // BL
        {uvRect.z, uvRect.y}, // BR
        {uvRect.z, uvRect.w}, // TR
        {uvRect.x, uvRect.w}  // TL
    };

    glm::vec2 positions[4];
    for (int i = 0; i < 4; ++i) {
        glm::vec4 p = transform * glm::vec4(local[i], 0.0f, 1.0f);
        positions[i] = glm::vec2(p);
    }

    // check if adding this geometry would exceed capacity.
    // Prevent overflow: if adding this quad exceeds max capacity, flush first
    if (cpuBuffer.size() + 4 > vboCapacity ||
        cpuIndices.size() + 6 > eboCapacity) {
        flush();
    }

    size_t base = cpuBuffer.size();
    ensureCapacity(base + 4, cpuIndices.size() + 6);

    for (int i = 0; i < 4; ++i)
        cpuBuffer.push_back({ positions[i], uvs[i], color, texIndex });

    cpuIndices.push_back((uint32_t)base + 0);
    cpuIndices.push_back((uint32_t)base + 1);
    cpuIndices.push_back((uint32_t)base + 2);
    cpuIndices.push_back((uint32_t)base + 2);
    cpuIndices.push_back((uint32_t)base + 3);
    cpuIndices.push_back((uint32_t)base + 0);
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

void Renderer::drawSprite(const Sprite& sprite) {
    drawQuad(sprite.pos, sprite.size, sprite.textureId,
        sprite.uv, sprite.color,
        sprite.rotation, sprite.uniformScale);
}