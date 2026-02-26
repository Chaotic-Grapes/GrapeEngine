#version 450 core

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform int uHasTexture;       // 1 = sample texture, 0 = flat color
uniform vec4 uColor;           // fallback flat color / tint

// MRT outputs (compatible with HDR pipeline)
layout(location = 0) out vec4 FragColor;

void main() {
    vec4 color;

    if (uHasTexture == 1) {
        color = texture(uTexture, vTexCoord) * uColor;
    } else {
        color = uColor;
    }

    // Discard fully transparent pixels
    if (color.a < 0.01) discard;

    FragColor = color;
}
