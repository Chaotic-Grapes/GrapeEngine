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
#include "graphics/TextureFilter.h"

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

    // Texture dimensions for aspect ratio correction
    int textureWidth = 0;
    int textureHeight = 0;

    // Material2D fields
    GLuint normalTextureId = 0;
    GLuint mraTextureId = 0;
    float metallic = 0.0f;
    float smoothness = 0.5f;
    float aoStrength = 1.0f;
    float normalStrength = 1.0f;
    uint32_t materialFlags = 0;

    Graphics::TextureFilter textureFilter = Graphics::TextureFilter::Nearest;

    // Apply debug/runtime transform input controls to this sprite instance.
    void handleInput(GLFWwindow* window, float deltaTime);
};

// Animation helper
class SpriteAnimation {
public:
    // Build animation metadata for a spritesheet texture.
    SpriteAnimation(GLuint texId, int frameWidth, int frameHeight, int texWidth, int texHeight);
    // Advance animation time and update the current frame index.
    void update(float deltaTime);
    // Return UV rectangle for the current frame.
    glm::vec4 currentUV() const;
    // Build a Sprite for the current frame placed at pos with the given size.
    Sprite currentFrame(const glm::vec2& pos, const glm::vec2& size) const;
    // Advance by dt and return a Sprite ready to render.
    Sprite play(glm::vec2 pos, glm::vec2 size, float dt);
    // Set playback speed in frames per second.
    void setFPS(float fps) { m_frameTime = 1.0f / fps; }
    // Restrict playback to a sub-range of frames in the sheet.
    void setFrameWindow(int startFrame, int count);
    // Select one row and optional sub-range for playback.
    void setRow(int rowIndex, int count = -1, int startCol = 0);
    // Set per-row frame counts for non-uniform spritesheets.
    void setRowFrameCounts(const std::vector<int>& counts);

    // Set the emissive texture and its HDR strength multiplier for this animation.
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
