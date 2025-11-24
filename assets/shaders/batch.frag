#version 460 core

in vec2  vTexCoord;
in vec4  vColor;
flat in float vTexIndex;  // flat to avoid interpolation glitches
uniform bool uPicking;

layout (binding = 0) uniform sampler2D uTextures[32];

// Multiple render targets for bloom
layout (location = 0) out vec4 FragColor;

void main() {
    if (uPicking) {
        // Textured sprites: if vTexIndex >= 0.0 -> sample and alpha-discard
        // Shapes / solid ID quads: vTexIndex < 0.0 -> no sampling, always pickable
        if (vTexIndex >= 0.0) {  // 0 is a valid texture slot
            vec4 texColor = texture(uTextures[int(vTexIndex)], vTexCoord);
            if (texColor.a < 0.1)
                discard;          // transparent pixel, no hit
        }
        FragColor = vColor;    // opaque pixel or shape, write ID color
        return;
    }

    if (vTexIndex < 0.0) {
        FragColor = vColor;
    } else {
        int index = int(vTexIndex);
        FragColor = texture(uTextures[index], vTexCoord) * vColor;
    }
}
