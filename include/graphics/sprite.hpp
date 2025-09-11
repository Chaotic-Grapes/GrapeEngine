#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Sprite {
    glm::vec2 pos;   // center position
    glm::vec2 size;  // width, height
    GLuint texture;  // OpenGL texture handle
};