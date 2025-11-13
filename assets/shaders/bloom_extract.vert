#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

// this shader’s only job is to map the texture coordinates correctly and 
// output clip-space positions for rendering the bloom extraction pass
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
