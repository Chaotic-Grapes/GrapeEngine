#version 460 core

in vec2 vTexCoord;
in vec4 vColor;
flat in float vTexIndex;

layout (location = 0) out vec4 FragColor;

// Albedo textures (slots 0-15)
layout (binding = 0) uniform sampler2D uTextures[16];

uniform float uGamma = 1.5;

void main() {
    vec4 baseColor = vColor;
    if (int(vTexIndex) >= 0) {
        baseColor *= texture(uTextures[int(vTexIndex)], vTexCoord);
    }

    if (baseColor.a < 0.01) {
        discard;
    }

    vec3 mapped = pow(max(baseColor.rgb, vec3(0.0)), vec3(1.0 / uGamma));
    FragColor = vec4(mapped, baseColor.a);
}
