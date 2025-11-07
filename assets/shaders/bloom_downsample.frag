#version 460 core

in vec2  vUV;               // UV coordinate for sampling the source texture
out vec4 FragColor;         // Final color written to the framebuffer

uniform sampler2D uImage;   // The high-resolution image to be downsampled

// Helper: compute luminance (perceived brightness) of a color.
// The weights are from the Rec.709 standard.
float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

void main() {
    // Compute texel size (1 / texture resolution)
    // This gives the spacing between neighboring samples in UV space
    vec2 texel = 1.0 / vec2(textureSize(uImage, 0));

    // Offsets are chosen to sample 4 points near the current UV.
    // Because bilinear filtering blends surrounding texels,
    // these 4 taps approximate the average of a 4x4 region.
    vec2 o = texel * 0.5;

    // Sample four neighboring texels
    vec3 c00 = texture(uImage, vUV + vec2(-o.x, -o.y)).rgb;
    vec3 c10 = texture(uImage, vUV + vec2( +o.x, -o.y)).rgb;
    vec3 c01 = texture(uImage, vUV + vec2(-o.x, +o.y)).rgb;
    vec3 c11 = texture(uImage, vUV + vec2( +o.x, +o.y)).rgb;

    // clamp for HDR data
    // Prevents single overbright pixels (like stars) from
    // bleeding excessively into lower-resolution mip levels
    vec3 meanCol = (c00 + c10 + c01 + c11) * 0.25;
    float meanLum = luminance(meanCol);
    float clampMax = max(1.0, meanLum * 12.0); // tweak: larger => softer clamp

    // Clamp each color to that maximum brightness
    c00 = min(c00, clampMax);
    c10 = min(c10, clampMax);
    c01 = min(c01, clampMax);
    c11 = min(c11, clampMax);

    // Average the four (possibly clamped) samples.
    // This produces the downsampled color for the current pixel.
    vec3 down = (c00 + c10 + c01 + c11) * 0.25;

    // Alpha is fixed to 1.0 because we're generating a color image.
    FragColor = vec4(down, 1.0);
}
