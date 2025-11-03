#version 450 core
layout (location = 0) out vec4 FragColor;

in vec2 vTexCoord;
uniform sampler2D uImage;
uniform bool  uHorizontal;
uniform float uFalloff; // e.g. 0.1–0.3 typical range

// Tunables
uniform float uRadius;   // in *texels* of this buffer (e.g., 8.0 at half-res ~ 16px full-res)
uniform int   uSamples;  // number of sample *pairs* (not counting center). Try 6–10.

// --- Gaussian helper ----
float gaussian(float x, float sigma) {
    return exp(-0.5 * (x * x) / (sigma * sigma));
}

// Applies a separable Gaussian blur in one direction (horizontal or vertical)
void main() {
    // Axis unit step in UV for one texel
    vec2 texel = 1.0 / vec2(textureSize(uImage, 0));
    vec2 axis  = uHorizontal ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);

    // Map radius to sigma. Sigma ~ radius * 0.5 is a good starting point.
    float sigma = max(uRadius * 0.5, 1e-4);

    // Evenly spaced samples from 0..uRadius (in texels)
    // step = how far apart each tap is, in texels along the chosen axis.
    int   N    = max(uSamples, 1);
    float step = max(uRadius / float(N), 1e-4);

    // Center sample
    float w0 = gaussian(0.0, sigma);
    vec3  acc = texture(uImage, vTexCoord).rgb * w0;
    float wsum = w0;

    // Symmetric pairs
    // MAX_S is the compile-time cap; uSamples selects how many are used at runtime.
    const int MAX_S = 12;  // safe upper bound; driver will unroll
    for (int i = 1; i <= MAX_S; ++i) {
        if (i > N) break;

        float dist = step * float(i);                                       // distance in texels
        float wi = gaussian(dist, sigma) / (1.0 + uFalloff * dist * dist);  // Divide to slightly reduce contribution of far taps.
        vec2  off  = axis * dist;

        acc  += texture(uImage, vTexCoord + off).rgb * wi;
        acc  += texture(uImage, vTexCoord - off).rgb * wi;
        wsum += 2.0 * wi;
    }

    vec3 result = acc / wsum;       // Normalize accumulated color so total weight sums to 1.
    FragColor = vec4(result, 1.0);
}
