#version 450 core

in vec2 vTexCoord;
in vec4 vColor;
flat in float vTexIndex;

out vec4 FragColor;

void main()
{
    // For W1 milestone: ignore textures, just use vertex color
    FragColor = vColor;
}
