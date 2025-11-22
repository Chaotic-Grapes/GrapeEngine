#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTexIndex;
layout(location = 5) in float aEmissiveTexIndex;   // (skip 4, that's strokePx in other shader)
layout(location = 6) in float aEmissiveStrength;

uniform mat4 uViewProj;

out vec2 vTexCoord;   
out vec4 vColor;      
flat out float vTexIndex;
out float vEmissiveTexIndex;
out float vEmissiveStrength;

void main() {
    gl_Position = uViewProj * vec4(aPos.xy, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor    = aColor;
    vTexIndex = aTexIndex;
    vEmissiveTexIndex = aEmissiveTexIndex;
    vEmissiveStrength = aEmissiveStrength;
}