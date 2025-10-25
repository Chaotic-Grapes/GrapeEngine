#version 460 core

in vec2  vTexCoord;
in vec4  vColor;
flat in float vTexIndex;  // flat to avoid interpolation glitches

layout (binding = 0) uniform sampler2D uTextures[32];

// Multiple render targets for bloom
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

void main() {
    if (vTexIndex < 0.0) {              // -1.0 means no texture
        FragColor = vColor;
    } else {
        int index = int(vTexIndex);
        FragColor = texture(uTextures[index], vTexCoord) * vColor;
    }

    // Bloom extraction logic
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0)
        BrightColor = vec4(FragColor.rgb, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}