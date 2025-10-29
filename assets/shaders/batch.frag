#version 460 core

in vec2  vTexCoord;
in vec4  vColor;
flat in float vTexIndex;  // flat to avoid interpolation glitches

layout (binding = 0) uniform sampler2D uTextures[32];

// Multiple render targets for bloom
layout (location = 0) out vec4 FragColor;

void main() {
    if (vTexIndex < 0.0) {
        FragColor = vColor;
    } else {
        int index = int(vTexIndex);
        FragColor = texture(uTextures[index], vTexCoord) * vColor;
    }
}
