#version 460 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;

// godray_composite.frag
uniform sampler2D uGodRays;
uniform sampler2D uOccluder;
uniform vec3 uTint;
uniform float uStrength;

void main()
{
    vec2 uv = vTexCoord;
    vec3 rays = texture(uGodRays, uv).rgb;

    // Where the occluder mask is black (object present), suppress rays.
    // Where it's white (empty space), let rays through fully.
    float passthrough = texture(uOccluder, uv).r;

    FragColor = vec4(rays * uTint * uStrength * passthrough, 0.0);
}
