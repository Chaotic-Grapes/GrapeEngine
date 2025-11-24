/* Start Header *****************************************************************/
/*!
\file   sprite.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Implements the Sprite and SpriteAnimation classes for handling sprite rendering
and frame-based animation. SpriteAnimation manages timing, frame windows, and UV
calculations for spritesheets, while Sprite provides simple per-instance
properties like position, size, color, rotation, and scale.

Features:
- SpriteAnimation supports uniform frame timing and row-based animations.
- Computes UV coordinates for the current animation frame.
- Allows custom frame windows and per-row frame counts.
- Provides utility to build a Sprite for the current animation frame.
- Sprite includes basic input handling for rotation and scaling.
*/
/* End Header *******************************************************************/

#include "graphics/sprite.hpp"
#include <GLFW/glfw3.h>

// ctor: figure out how many rows/cols the spritesheet has
SpriteAnimation::SpriteAnimation(GLuint texId, int frameWidth, int frameHeight, int texWidth, int texHeight)
    : m_textureId(texId),
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight),
    m_texWidth(texWidth),
    m_texHeight(texHeight)
{
    m_totalCols = texWidth / frameWidth;
    m_totalRows = texHeight / frameHeight;

    m_windowStart = 0;
    m_windowCount = m_totalCols * m_totalRows;
    m_localFrame = 0;
}

// advance frame timer
void SpriteAnimation::update(float deltaTime) {
    m_accum += deltaTime;
    while (m_accum >= m_frameTime) {
        m_accum -= m_frameTime;
        m_localFrame = (m_localFrame + 1) % m_windowCount;
    }
}

// compute UV rect for current frame
glm::vec4 SpriteAnimation::currentUV() const {
    int absoluteFrame = m_windowStart + m_localFrame;

    int col = absoluteFrame % m_totalCols;
    int row = absoluteFrame / m_totalCols;

    float u0 = (col * m_frameWidth) / (float)m_texWidth;
    float v0 = (row * m_frameHeight) / (float)m_texHeight;
    float u1 = ((col + 1) * m_frameWidth) / (float)m_texWidth;
    float v1 = ((row + 1) * m_frameHeight) / (float)m_texHeight;

    return { u0, v0, u1, v1 };
}

void SpriteAnimation::setFrameWindow(int startFrame, int count) {
    int total = m_totalCols * m_totalRows;
    if (startFrame < 0) startFrame = 0;
    if (startFrame >= total) startFrame = total - 1;
    if (count <= 0) count = 1;
    if (startFrame + count > total) count = total - startFrame;

    m_windowStart = startFrame;
    m_windowCount = count;
    m_localFrame = 0;
}

void SpriteAnimation::setRow(int rowIndex, int count, int startCol) {
    if (rowIndex < 0 || rowIndex >= m_totalRows) return;

    int available = m_totalCols;
    if (!m_rowFrameCounts.empty() && rowIndex < (int)m_rowFrameCounts.size()) {
        available = m_rowFrameCounts[rowIndex];  // override with actual count
    }

    // clamp startCol
    if (startCol >= available) startCol = available - 1;

    int start = rowIndex * m_totalCols + startCol;
    int maxCount = available - startCol;

    if (count < 0 || count > maxCount) count = maxCount;

    setFrameWindow(start, count);
}

// Build a Sprite for the current frame
Sprite SpriteAnimation::currentFrame(const glm::vec2& pos, const glm::vec2& size) const {
    Sprite s;
    s.pos = pos;
    s.size = size;
    s.textureId = m_textureId;
    s.uv = currentUV();
    s.color = { 1, 1, 1, 1 };

    // Pass through emissive data
    s.emissiveTextureId = m_emissiveTextureId;
    s.emissiveStrength = m_emissiveStrength;

    return s;
}

Sprite SpriteAnimation::play(glm::vec2 pos, glm::vec2 size, float dt) {
    update(dt);
    return currentFrame(pos, size);
}

void Sprite::handleInput(GLFWwindow* window, float deltaTime) {
    const float rotateSpeed = 2.0f;  // radians per second
    const float scaleSpeed = 1.0f;  // units per second

    // Rotate CCW with R
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        rotation += rotateSpeed * deltaTime;
    }

    // Zoom out (J)
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
        uniformScale -= scaleSpeed * deltaTime;
        if (uniformScale < 0.1f) uniformScale = 0.1f; // clamp
    }

    // Zoom in (K)
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        uniformScale += scaleSpeed * deltaTime;
    }
}

void SpriteAnimation::setRowFrameCounts(const std::vector<int>& counts) {
    m_rowFrameCounts = counts;
}