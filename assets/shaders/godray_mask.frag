#version 460 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uScene;       // HDR scene input
uniform float uThreshold;       // luminance below this = occluder (black)

void main()
{
    vec3 color = texture(uScene, vTexCoord).rgb;
    float lum = dot(color, vec3(0.299, 0.587, 0.114));

    // Objects are brighter than the near-black background,
    // so anything above threshold is an occluder (black),
    // empty space is light (white)
    float isOccluder = step(uThreshold, lum);
    float mask = 1.0 - isOccluder;

    FragColor = vec4(vec3(mask), 1.0);
}
