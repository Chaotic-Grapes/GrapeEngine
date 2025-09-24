#pragma once
#include <glm/glm.hpp>

// Per-instance data for instanced rendering
struct InstanceData {
    glm::vec2 position;   // center of sprite
    glm::vec2 size;       // width/height
    glm::vec4 color;      // tint
    glm::vec4 uv;         // {u0, v0, u1, v1}
    float texIndex;       // texture slot index
    float _padding[3];    // pad to 16-byte alignment (std140/std430)
};

// Padding is necessary for OpenGL uniform buffer objects (UBOs) 
// or shader storage buffer objects (SSBOs) that follow std140 or std430 layout rules.