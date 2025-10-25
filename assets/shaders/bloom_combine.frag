#version 450 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uScene;      // HDR color texture
uniform sampler2D uBloomBlur;  // blurred bright areas
uniform float uExposure = 1.0; // tweak as desired

void main() {
    const float gamma = 2.2;
    vec3 hdrColor = texture(uScene, vTexCoord).rgb;
    vec3 bloomColor = texture(uBloomBlur, vTexCoord).rgb;

    // Additively blend bloom before tone mapping
    vec3 color = hdrColor + bloomColor;

    // Tone map (exponential)
    vec3 mapped = vec3(1.0) - exp(-color * uExposure);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));

    FragColor = vec4(mapped, 1.0);
}
