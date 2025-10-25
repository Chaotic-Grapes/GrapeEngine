#version 450 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;
uniform sampler2D uImage;
uniform bool uHorizontal;

const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 tex_offset = 1.0 / textureSize(uImage, 0); // size of one texel
    vec3 result = texture(uImage, vTexCoord).rgb * weight[0];

    if (uHorizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(uImage, vTexCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(uImage, vTexCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(uImage, vTexCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(uImage, vTexCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
