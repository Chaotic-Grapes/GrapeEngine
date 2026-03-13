/* Start Header *****************************************************************/
/*!
\file   particle.vert
\author Choi Meng Yew (100%)
\date   13th March 2026
\brief
Vertex shader for instanced particle rendering in GrapeEngine.

Particles are simulated on the GPU using CUDA and stored in a shared
CUDA–OpenGL interoperable VBO. Each particle instance provides position,
velocity, lifetime, color, size, and rotation parameters packed into an
interleaved structure.

This shader expands a unit quad into a billboarded particle by applying
rotation and scaling per instance, then transforms the particle into
clip space using the view-projection matrix.

Features:
    - Instanced particle rendering
    - CUDA–OpenGL interop buffer layout
    - Per-particle rotation and size scaling
    - Per-particle color reconstruction
    - World position output for lighting calculations

Responsibilities:
    - Decode particle instance attributes from the interleaved buffer
    - Rotate and scale the particle quad
    - Translate particle to world position
    - Output particle color, UV coordinates, and world position

Used by:
    - ParticleSystem GPU renderer
    - CUDA particle simulation pipeline
    - GrapeEngine instanced particle renderer

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#version 460 core

// Per-vertex (unit quad)
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

// Per-instance (from interleaved CUDA VBO)
layout(location = 2) in vec4 aPosVel;     // pos.x, pos.y, vel.x, vel.y
layout(location = 3) in vec4 aLifeColor;  // life, maxLife, colorR, colorG
layout(location = 4) in vec4 aSizeRot;    // colorB, colorA, size, rotation

uniform mat4 uViewProj;
uniform float uParticleSize;

out vec2 vTexCoord;
out vec4 vColor;
out vec2 vWorldPos;

void main() {
    float size = aSizeRot.z * uParticleSize;
    float rotation = aSizeRot.w;

    // Rotate quad vertex
    float c = cos(rotation);
    float s = sin(rotation);
    vec2 rotated = vec2(
        aPos.x * c - aPos.y * s,
        aPos.x * s + aPos.y * c
    );

    vec2 worldPos = aPosVel.xy + rotated * size;

    gl_Position = uViewProj * vec4(worldPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vWorldPos = worldPos;

    // Reconstruct RGBA from interleaved data
    vColor = vec4(aLifeColor.z, aLifeColor.w, aSizeRot.x, aSizeRot.y);
}
