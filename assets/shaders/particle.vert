#version 450 core

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
