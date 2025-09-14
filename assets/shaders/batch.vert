#version 450 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTexIndex;

uniform mat4 uProjection;

out vec2 vTexCoord;   
out vec4 vColor;      
flat out float vTexIndex;

void main() {
    gl_Position = uProjection * vec4(aPos.xy, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor    = aColor;
    vTexIndex = aTexIndex;
}