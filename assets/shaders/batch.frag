#version 450 core

in vec2  vTexCoord;
in vec4  vColor;
flat in float vTexIndex;  // flat to avoid interpolation glitches

layout (binding = 0) uniform sampler2D uTextures[32];
out vec4 FragColor;

void main() {
    if (vTexIndex < 0.0) {              // -1.0 means no texture
        FragColor = vColor;
    } else {
        int index = int(vTexIndex);
        FragColor = texture(uTextures[index], vTexCoord) * vColor;
    }
}