#version 460 core

in vec2 vUV;
in vec4 vColor;
flat in int vTexIdx;

out vec4 FragColor;

// match the renderer's max texture slots
uniform sampler2D uTextures[32];

void main()
{
    float sdf = texture(uTextures[vTexIdx], vUV).r;

    // Smoothstep around 0.5 gives sharp but anti-aliased edges
    float dist = smoothstep(0.45, 0.55, sdf);

    FragColor = vec4(vColor.rgb, vColor.a * dist);

    // optional: discard fully transparent pixels (faster on some GPUs)
    if (FragColor.a < 0.01)
        discard;
}