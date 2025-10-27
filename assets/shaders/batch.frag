#version 460 core

in vec2  vTexCoord;
in vec4  vColor;
flat in float vTexIndex;  // flat to avoid interpolation glitches

layout (binding = 0) uniform sampler2D uTextures[32];

// Multiple render targets for bloom
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

void main() {
    // ------------------------------------------------------------
    // Choose between solid color or texture
    // ------------------------------------------------------------
    if (vTexIndex < 0.0) {              // -1.0 means no texture
        FragColor = vColor;
    } else {
        int index = int(vTexIndex);
        FragColor = texture(uTextures[index], vTexCoord) * vColor;
    }

    // ------------------------------------------------------------
    // Bloom extraction
    // ------------------------------------------------------------
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Anything brighter than this threshold contributes to bloom
    // We can tweak 1.0 => 0.8 for more aggressive glow
    BrightColor = (brightness > 1.0)
        ? vec4(FragColor.rgb, 1.0)
        : vec4(0.0, 0.0, 0.0, 1.0);
}
