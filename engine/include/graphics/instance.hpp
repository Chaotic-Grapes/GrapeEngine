/* Start Header *****************************************************************/
/*!
\file   instance.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\brief
Declares per-instance GPU payload layout used by instanced rendering paths.

The struct layout is designed to remain alignment-safe for std140/std430-like
buffer packing expectations when consumed by shader-side instance data.
*/
/* End Header *******************************************************************/

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