/* Start Header *****************************************************************/
/*!
\file   boid.vert
\author Choi Meng Yew (100%)
\date   13th March 2026
\brief
Vertex shader for instanced boid rendering.

This shader renders each boid as a rotated quad using instanced data
provided by a CUDA–OpenGL interoperable VBO. Each instance contains the
boid's position and velocity direction, allowing the quad to orient
itself along the direction of movement.

Inputs:
    - Per-vertex unit quad geometry (aPos, aTexCoord)
    - Per-instance boid data (position + velocity direction)

Responsibilities:
    - Compute boid rotation from velocity vector
    - Rotate and scale the quad to face movement direction
    - Translate the quad to the boid's world position
    - Output world position and texture coordinates for fragment shader

Used by:
    - BoidSystem instanced renderer
    - CUDA–OpenGL boid simulation pipeline

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#version 460 core

// Per-vertex (unit quad)
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

// Per-instance (from CUDA interop VBO)
layout(location = 2) in vec4 aInstancePosVel;  // xy = position, zw = velocity direction

uniform mat4  uViewProj;
uniform float uBoidSize;

out vec2 vTexCoord;
out vec3 vWorldPos;

void main() {
    vTexCoord = aTexCoord;

    // Compute rotation from velocity direction
    vec2  vel  = aInstancePosVel.zw;
    float len  = length(vel);

    float cosA, sinA;
    if (len > 0.001) {
        vec2 dir = vel / len;
        cosA = dir.x;
        sinA = dir.y;
    } else {
        cosA = 1.0;
        sinA = 0.0;
    }

    // Rotate and scale the quad vertex
    vec2 scaled  = aPos * uBoidSize;
    vec2 rotated = vec2(
        scaled.x * cosA - scaled.y * sinA,
        scaled.x * sinA + scaled.y * cosA
    );

    // Translate to world position
    vec2 worldPos = aInstancePosVel.xy + rotated;

    vWorldPos  = vec3(worldPos, 0.0);
    gl_Position = uViewProj * vec4(worldPos, 0.0, 1.0);
}