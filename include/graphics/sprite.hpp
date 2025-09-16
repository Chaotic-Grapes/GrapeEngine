#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Sprite {
    glm::vec2 pos;              // center position
    glm::vec2 size;             // width, height
    glm::vec4 uv;               // {u0, v0, u1, v1}  -> top-left and bottom-right
    glm::vec4 color{ 1,1,1,1 }; // default tint (white = no tint)
    GLuint textureId;           // OpenGL texture handle
};

// Animation helper
class SpriteAnimation {
public:
    SpriteAnimation(GLuint texId, int frameWidth, int frameHeight, int texWidth, int texHeight);

    void update(float deltaTime);  // advance frame timer
    glm::vec4 currentUV() const;   // returns {u0, v0, u1, v1} for renderer
    Sprite currentFrame(const glm::vec2& pos, const glm::vec2& size) const;
    Sprite play(glm::vec2 pos, glm::vec2 size, float dt);

    void setFPS(float fps) { m_frameTime = 1.0f / fps; }

private:
    GLuint m_textureId;
    int m_frameWidth, m_frameHeight;
    int m_texWidth, m_texHeight;

    int m_totalCols, m_totalRows;
    int m_currentFrame = 0;
    float m_frameTime = 0.1f;  // default 10 FPS
    float m_accum = 0.0f;
};