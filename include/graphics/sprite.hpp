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

    void handleInput(GLFWwindow* window, float deltaTime);
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

    void setFrameWindow(int startFrame, int count);         // play any [start..start+count)
    void setRow(int rowIndex, int count = -1, int startCol = 0); // play one row (optionally subset)

    void setRowFrameCounts(const std::vector<int>& counts);

private:
    GLuint m_textureId;
    int m_frameWidth, m_frameHeight;
    int m_texWidth, m_texHeight;

    int m_totalCols, m_totalRows;


    int m_windowStart = 0;   // first absolute frame index
    int m_windowCount = 0;   // number of frames in the window
    int m_localFrame = 0;    // index inside the window
    std::vector<int> m_rowFrameCounts; // store explicit frame counts per row

    // legacy, can be removed once we switch fully to windowed playback
    int m_currentFrame = 0;

    float m_frameTime = 0.1f;  // default 10 FPS
    float m_accum = 0.0f;
};