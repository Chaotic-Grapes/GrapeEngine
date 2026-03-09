#version 450 core

// Per-vertex (unit quad)
layout(location = 0) in vec2 aPos;      // quad corner: (-0.5,-0.5) to (0.5,0.5)
layout(location = 1) in vec2 aTexCoord; // UV

// Per-instance (from CUDA interop VBO)
layout(location = 2) in vec4 aInstancePosVel;  // xy = position, zw = velocity direction

uniform mat4 uViewProj;
uniform float uBoidSize;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;

    // Compute rotation from velocity direction
    vec2 vel = aInstancePosVel.zw;
    float len = length(vel);

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
    vec2 scaled = aPos * uBoidSize;
    vec2 rotated = vec2(
        scaled.x * cosA - scaled.y * sinA,
        scaled.x * sinA + scaled.y * cosA
    );

    // Translate to world position
    vec2 worldPos = aInstancePosVel.xy + rotated;

    gl_Position = uViewProj * vec4(worldPos, 0.0, 1.0);
}
