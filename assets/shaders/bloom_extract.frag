#version 460 core

out vec4 FragColor;        // Final output color (only bright areas)
in vec2 vTexCoord;         // Interpolated UV coordinate

uniform sampler2D uScene;   // Input HDR scene texture
uniform float uThreshold;   // Brightness threshold for bloom extraction

void main()
{
    // Sample the HDR scene color at the current fragment
    vec3 color = texture(uScene, vTexCoord).rgb;
    
    // Compute perceived brightness using Rec.709 luminance weights.
    // These coefficients define how much each RGB channel contributes to *perceived brightness*,
    // not to physical light energy. In human vision, green light contributes about 71% of the
    // perceived luminance, red about 21%, and blue only around 7%.
    // This weighting matches how the human eye responds to color intensity.
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Apply a soft threshold (soft-knee function) to avoid harsh cutoffs
    float knee = 0.5;                               // Controls how soft the threshold edge is
    float soft = brightness - uThreshold + knee; 
    soft = clamp(soft, 0.0, 2.0 * knee);            // Limit to a reasonable transition range
    soft = soft * soft / (4.0 * knee + 0.00001);    // Shape into a smooth curve
    
    // Combine the hard and soft threshold components
    float contribution = max(soft, brightness - uThreshold);

    // Normalize so that brightness scaling is consistent and safe from division by zero
    contribution /= max(brightness, 0.00001);
    
    // Multiply original color by this contribution to keep only the bright parts
    vec3 bloom = color * contribution;

    // Output the extracted bloom color (alpha fixed to 1)
    FragColor = vec4(bloom, 1.0);
}
