#version 460 core

in vec2 vTexCoord;
in vec4 vColor;
flat in float vTexIndex;
in float vEmissiveTexIndex;
in float vEmissiveStrength;

uniform bool uPicking;

// Albedo textures in slots 0-23
layout (binding = 0) uniform sampler2D uTextures[24];
// Emissive textures in slots 24-31 (binding starts at 24)
layout (binding = 24) uniform sampler2D uEmissiveTextures[8];

// Multiple render targets for bloom
layout (location = 0) out vec4 FragColor;

void main() {
    if (uPicking) {
        // Textured sprites: if vTexIndex >= 0.0 -> sample and alpha-discard
        // Shapes / solid ID quads: vTexIndex < 0.0 -> no sampling, always pickable
        if (vTexIndex >= 0.0) {  // 0 is a valid texture slot
            vec4 texColor = texture(uTextures[int(vTexIndex)], vTexCoord);
            if (texColor.a < 0.1)
                discard;          // transparent pixel, no hit
        }
        FragColor = vColor;    // opaque pixel or shape, write ID color
        return;
    }

    // Normal rendering - sample albedo
    vec4 albedo;
    if (vTexIndex < 0.0) {
        albedo = vColor;
    } else {
        int index = int(vTexIndex);
        albedo = texture(uTextures[index], vTexCoord) * vColor;
    }
    
    // Sample emissive and add contribution
    vec3 emission = vec3(0.0);
    int emissiveIdx = int(vEmissiveTexIndex);
    
if (emissiveIdx >= 0 && vEmissiveStrength > 0.0) {
    float emissiveMask = texture(uEmissiveTextures[emissiveIdx], vTexCoord).r;
    
    // Get the hue from albedo
    vec3 albedoColor = albedo.rgb;
    
    // Calculate perceived brightness (luminance)
    float luminance = dot(albedoColor, vec3(0.2126, 0.7152, 0.0722));
    
    // Normalize to constant luminance
    if (luminance > 0.001) {
        albedoColor = albedoColor / luminance;  // All colors now have luminance = 1.0
    }
    
    emission = albedoColor * emissiveMask * vEmissiveStrength;
}
    
    // Output HDR color (albedo + emission)
    FragColor = vec4(albedo.rgb + emission, albedo.a);
}