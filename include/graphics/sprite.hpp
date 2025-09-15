#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Sprite {
    glm::vec2 pos;        // center position
    glm::vec2 size;       // width, height
    glm::vec2 texCoords[4]; // per-vertex UVs
    GLuint textureId;     // OpenGL texture handle
};