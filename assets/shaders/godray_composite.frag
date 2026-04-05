#version 460 core
layout (location = 0) out vec4 FragColor;

in vec2 vUV;

// godray_composite.frag
uniform sampler2D uGodRays;
uniform sampler2D uOccluder;
uniform vec3 uTint;
uniform float uStrength;

void main()
{
    vec3 rays = texture(uGodRays, vUV).rgb;
    FragColor = vec4(rays * uTint * uStrength, 0.0);
}
