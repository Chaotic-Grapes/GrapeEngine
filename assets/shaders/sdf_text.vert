#version 460 core

layout(location = 0) in vec2 aPos;      // vertex position
layout(location = 1) in vec2 aUV;       // tex coords
layout(location = 2) in vec4 aColor;    // per-vertex color
layout(location = 3) in float aTexIdx;  // texture slot index

out vec2 vUV;
out vec4 vColor;
flat out int vTexIdx;

uniform mat4 uProjection;

void main()
{
    vUV = aUV;
    vColor = aColor;
    vTexIdx = int(aTexIdx);

    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}