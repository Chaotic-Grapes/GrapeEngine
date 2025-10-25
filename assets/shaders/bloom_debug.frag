#version 450 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;
uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoord);
}
