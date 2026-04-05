#version 460 core
layout (location = 0) out vec4 FragColor;

in vec2 vUV;

uniform sampler2D uScene;
uniform float uThreshold;

void main()
{
    vec3 color = texture(uScene, vUV).rgb;
    float lum = dot(color, vec3(0.299, 0.587, 0.114));

    // Objects are occluders (black), empty space lets light through (white)
    float isOccluder = step(uThreshold, lum);
    float mask = 1.0 - isOccluder;

    FragColor = vec4(vec3(mask), 1.0);
}