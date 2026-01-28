#version 460 core

in vec2 vTexCoord;
in vec4 vColor;
flat in float vTexIndex;
in float vEmissiveTexIndex;
in float vEmissiveStrength;

// required for point lights
// must output this from batch.vert (world-space or view-space, just be consistent).
in vec3 vWorldPos;

uniform bool uPicking;

// lighting toggle (defaults to 0 if you never set it -> keeps old look)
uniform int uLightingEnabled;

// Albedo textures in slots 0-23
layout (binding = 0) uniform sampler2D uTextures[24];
// Emissive textures in slots 24-31 (binding starts at 24)
layout (binding = 24) uniform sampler2D uEmissiveTextures[8];

// ============================================================================
// Light data (MUST MATCH LightManager Bind() names / SSBO binding points)
// ============================================================================

struct GPUPointLight {
    vec4 PositionAndRange;    // xyz = position, w = range
    vec4 ColorAndIntensity;   // rgb = color,  a = intensity
};

// SSBO bound by LightManager at binding point 0
layout(std430, binding = 0) buffer PointLightBuffer {
    GPUPointLight uPointLights[];
};

struct DirLight {
    vec3 Direction;
    vec3 Color;
    float Intensity;
};

uniform int uPointLightCount;
uniform int uHasDirectional;
uniform DirLight uDirLight;

// Simple 2D “surface normal” (sprites are effectively facing camera)
vec3 GetDefaultNormal2D()
{
    return vec3(0.0, 0.0, 1.0);
}

// Distance attenuation with a soft edge near range
float Attenuation(float dist, float range)
{
    // If range is 0, avoid division issues
    float r = max(range, 1e-4);
    float x = clamp(dist / r, 0.0, 1.0);

    // Smooth falloff to 0 at the edge
    float falloff = 1.0 - x;
    falloff *= falloff; // quadratic
    return falloff;
}

vec3 ApplyForwardLighting(vec3 baseColor, vec3 worldPos)
{
    vec3 N = GetDefaultNormal2D();

    // Small ambient so unlit areas aren't black (tweak later)
    vec3 accum = vec3(0.08);

    // Directional light (treat Direction as "light direction", pointing FROM light TO scene)
    if (uHasDirectional != 0) {
        vec3 L = normalize(-uDirLight.Direction); // invert so L points from surface -> light
        float ndotl = max(dot(N, L), 0.0);
        accum += (uDirLight.Color * uDirLight.Intensity) * ndotl;
    }

    // Point lights
    int count = max(uPointLightCount, 0);
    for (int i = 0; i < count; ++i) {
        vec3 lp = uPointLights[i].PositionAndRange.xyz;
        float range = uPointLights[i].PositionAndRange.w;

        vec3 toL = lp - worldPos;
        float dist = length(toL);

        // Early skip if outside range
        if (dist > range) continue;

        vec3 L = (dist > 1e-5) ? (toL / dist) : vec3(0.0, 0.0, 1.0);
        float ndotl = max(dot(N, L), 0.0);

        vec3 lightColor = uPointLights[i].ColorAndIntensity.rgb;
        float intensity = uPointLights[i].ColorAndIntensity.a;

        float att = Attenuation(dist, range);
        accum += lightColor * intensity * ndotl * att;
    }

    // Final lit color (Lambert-ish)
    return baseColor * accum;
}

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

    // Apply lighting only if enabled (keeps old output if you never set it)
    vec3 baseRgb = albedo.rgb;
    if (uLightingEnabled != 0) {
        baseRgb = ApplyForwardLighting(baseRgb, vWorldPos);
    }
    
    // Output HDR color (lit albedo + emission)
    FragColor = vec4(baseRgb + emission, albedo.a);
}
