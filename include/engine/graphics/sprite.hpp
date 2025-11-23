/* Start Header *****************************************************************/
/*!
\file   sprite.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Declares the Sprite struct and SpriteAnimation class for 2D rendering.

Sprite:
- Represents a single textured quad in world space.
- Stores position, size, UV coordinates, color tint, rotation, and scale.
- Provides basic input handling for rotation and scaling via GLFW.

SpriteAnimation:
- Utility class for handling spritesheet-based animations.
- Manages frame timing, UV calculation, and playback windows.
- Supports row-based animations, custom frame windows, and per-row frame counts.
- Provides methods to update animation state and build a Sprite for the
  current frame.

*/
/* End Header *******************************************************************/

#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>
#include <GLFW/glfw3.h>

struct Sprite {
    glm::vec2 pos;              // center position
    glm::vec2 size;             // width, height
    glm::vec4 uv;               // {u0, v0, u1, v1}  -> top-left and bottom-right
    glm::vec4 color{ 1,1,1,1 }; // default tint (white = no tint)
    GLuint textureId;           // OpenGL texture handle
    float rotation = 0.0f;      // in radians
    float uniformScale = 1.0f;  // scaling

    // Emissive support
    GLuint emissiveTextureId = 0;   // 0 = no emissive map
    float emissiveStrength = 0.0f;  // HDR multiplier

    void handleInput(GLFWwindow* window, float deltaTime);
};

// Animation helper
class SpriteAnimation {
public:
    SpriteAnimation(GLuint texId, int frameWidth, int frameHeight, int texWidth, int texHeight);
    void update(float deltaTime);
    glm::vec4 currentUV() const;
    Sprite currentFrame(const glm::vec2& pos, const glm::vec2& size) const;
    Sprite play(glm::vec2 pos, glm::vec2 size, float dt);
    void setFPS(float fps) { m_frameTime = 1.0f / fps; }
    void setFrameWindow(int startFrame, int count);
    void setRow(int rowIndex, int count = -1, int startCol = 0);
    void setRowFrameCounts(const std::vector<int>& counts);

    // NEW: Emissive support
    void setEmissive(GLuint emissiveTexId, float strength) {
        m_emissiveTextureId = emissiveTexId;
        m_emissiveStrength = strength;
    }

private:
    GLuint m_textureId;
    int m_frameWidth, m_frameHeight;
    int m_texWidth, m_texHeight;
    int m_totalCols, m_totalRows;
    int m_windowStart = 0;
    int m_windowCount = 0;
    int m_localFrame = 0;
    std::vector<int> m_rowFrameCounts;
    int m_currentFrame = 0;
    float m_frameTime = 0.1f;
    float m_accum = 0.0f;

    // Emissive properties
    GLuint m_emissiveTextureId = 0;
    float m_emissiveStrength = 0.0f;
};
