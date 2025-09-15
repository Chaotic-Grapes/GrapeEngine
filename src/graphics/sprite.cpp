#include "../include/graphics/sprite.hpp"

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
}

// advance frame timer
void SpriteAnimation::update(float deltaTime) {
    m_accum += deltaTime;
    if (m_accum >= m_frameTime) {
        m_accum -= m_frameTime;
        m_currentFrame = (m_currentFrame + 1) % (m_totalCols * m_totalRows);
    }
}

// compute UV rect for current frame
glm::vec4 SpriteAnimation::currentUV() const {
    int col = m_currentFrame % m_totalCols;
    int row = m_currentFrame / m_totalCols;

    float u0 = (col * m_frameWidth) / (float)m_texWidth;
    float v0 = (row * m_frameHeight) / (float)m_texHeight;
    float u1 = ((col + 1) * m_frameWidth) / (float)m_texWidth;
    float v1 = ((row + 1) * m_frameHeight) / (float)m_texHeight;

    // Return (u0,v0,u1,v1)
    return { u0, v0, u1, v1 };
}

// Build a Sprite for the current frame
Sprite SpriteAnimation::currentFrame(const glm::vec2& pos, const glm::vec2& size) const {
    Sprite s;
    s.pos = pos;
    s.size = size;
    s.textureId = m_textureId;
    s.uv = currentUV();         // {u0, v0, u1, v1}
    s.color = { 1, 1, 1, 1 };   // default white tint
    return s;
}

Sprite SpriteAnimation::play(glm::vec2 pos, glm::vec2 size, float dt) {
    update(dt);
    return currentFrame(pos, size);
}