#version 460 core
layout(location = 0) in vec2  aPos;
layout(location = 1) in vec2  aUV;       // UVs in [-1, 1] define a square SDF domain
layout(location = 2) in vec4  aColor;    // vertex color
layout(location = 3) in float aTexIdx;   // unused in this shader
layout(location = 4) in float aStrokePx; // stroke thickness in pixels

out vec2  vUV;        // pass to fragment shader
out vec4  vColor;
out float vStrokePx;

uniform mat4 uViewProj; // transforms position to clip space

void main() {
    // Pass through per-vertex attributes
    vUV        = aUV;
    vColor     = aColor;
    vStrokePx  = aStrokePx;

    // Apply view-projection transform for correct positioning
    gl_Position = uViewProj * vec4(aPos, 0.0, 1.0);
}
