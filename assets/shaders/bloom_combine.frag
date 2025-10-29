#version 450 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uScene;           // HDR scene color
uniform sampler2D uBloomBlur;       // blurred bright regions (bloom texture)
uniform float uExposure = 1.0;      // overall brightness scaling
uniform float uBloomStrength = 0.2; // how intense the bloom glow is
uniform float uGamma = 2.2;         // gamma correction value

void main() {
    // Sample HDR scene and bloom textures
    vec3 hdrColor = texture(uScene, vTexCoord).rgb;
    vec3 bloomColor = texture(uBloomBlur, vTexCoord).rgb;

    // Combine bloom with controllable intensity
    vec3 color = hdrColor + bloomColor * uBloomStrength;

    // Apply exposure adjustment (simulate camera exposure)
    color *= uExposure;

    // Reinhard tone mapping to compress HDR range into [0,1]
    vec3 mapped = color / (color + vec3(1.0));

    // Gamma correction to convert linear color to sRGB space
    mapped = pow(mapped, vec3(1.0 / uGamma));

    // Output final LDR color
    FragColor = vec4(mapped, 1.0);
}
