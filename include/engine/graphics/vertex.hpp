#pragma once
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec4 color;
    float     texIndex; // filled by renderer when you pass a textureId
};