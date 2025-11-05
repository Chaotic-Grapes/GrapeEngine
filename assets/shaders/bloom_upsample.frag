#version 460 core

in vec2  vUV;
out vec4 FragColor;

uniform sampler2D uLowMip;   // Lower-resolution mip level (blurred source)
uniform sampler2D uHighMip;  // Higher-resolution base image to be blended into
uniform float uBlend = 1.0;  // Blend factor controlling how much the low mip adds

void main() {
    // Sample the higher-resolution image (base level)
    vec3 high = texture(uHighMip, vUV).rgb;

    // Determine texel size for the *lower-resolution* texture.
    // We use this to offset our sampling positions.
    vec2 texelLow = 1.0 / vec2(textureSize(uLowMip, 0));

    // Apply a 9-tap tent filter to the low-resolution image
    vec2 o = texelLow;
    vec3 up =
          texture(uLowMip, vUV + vec2(-o.x, -o.y)).rgb * 1.0 +
          texture(uLowMip, vUV + vec2( 0.0 , -o.y)).rgb * 2.0 +
          texture(uLowMip, vUV + vec2( o.x , -o.y)).rgb * 1.0 +

          texture(uLowMip, vUV + vec2(-o.x,  0.0)).rgb * 2.0 +
          texture(uLowMip, vUV + vec2( 0.0 ,  0.0)).rgb * 4.0 +
          texture(uLowMip, vUV + vec2( o.x ,  0.0)).rgb * 2.0 +

          texture(uLowMip, vUV + vec2(-o.x,  o.y)).rgb * 1.0 +
          texture(uLowMip, vUV + vec2( 0.0 ,  o.y)).rgb * 2.0 +
          texture(uLowMip, vUV + vec2( o.x ,  o.y)).rgb * 1.0;

    // Normalize the weights so total = 1.0
    up *= (1.0 / 16.0);

    // Additively blend the blurred low mip back into the
    // high-resolution image. This forms the final bloom glow
    vec3 outCol = high + up * uBlend;

    // Output final color (fully opaque)
    FragColor = vec4(outCol, 1.0);
}
