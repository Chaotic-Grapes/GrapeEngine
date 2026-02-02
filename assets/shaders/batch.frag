#version 460 core

in vec2 vTexCoord;
in vec4 vColor;
flat in float vTexIndex;
flat in float vEmissiveTexIndex;
in float vEmissiveStrength;
flat in float vNormalTexIndex;
flat in float vMRATexIndex;
in vec4 vMaterialParams;  // x=Metallic, y=Smoothness, z=AOStrength, w=NormalStrength
flat in float vMaterialFlags;
in vec3 vWorldPos;        // World position for lighting (vec3 with z=0)

uniform int uPicking;
uniform int uLightingEnabled;

// ============================================================================
// TEXTURE SAMPLERS (32 slots total)
// ============================================================================
// Albedo textures (slots 0-15)
layout (binding = 0) uniform sampler2D uTextures[16];

// Normal maps (slots 16-21)
layout (binding = 16) uniform sampler2D uNormalTextures[6];

// MRA maps (slots 22-25)
layout (binding = 22) uniform sampler2D uMRATextures[4];

// Emissive textures (slots 26-31)
layout (binding = 26) uniform sampler2D uEmissiveTextures[6];

// ============================================================================
// Light data (MUST MATCH LightManager Bind() names / SSBO binding points)
// ============================================================================
struct GPUPointLight {
    vec4 PositionAndRange;    // xyz = position, w = range
    vec4 ColorAndIntensity;   // rgb = color,  a = intensity
};

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

// ============================================================================
// PBR Helper Functions
// ============================================================================
const float PI = 3.14159265359;
const vec3 DefaultNormal = vec3(0.0, 0.0, 1.0); // Up in tangent space

// GGX / Trowbridge-Reitz Normal Distribution Function (NDF)
// -------------------------------------------------------
// Describes how microfacet normals are statistically distributed on a surface.
// Controls the shape of specular highlights:
//
// - Low roughness  -> tightly aligned microfacets (sharp highlights)
// - High roughness -> widely distributed microfacets (broad highlights)
//
// GGX is used because it produces more realistic grazing-angle highlights and
// better matches measured real-world material behavior than older models.
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Schlick-GGX Geometry Function (Smith masking-shadowing)
// ------------------------------------------------------
// Approximates how microfacets block (mask) or shadow each other.
// Reduces specular reflection at grazing angles based on roughness,
// helping preserve energy and prevent overly bright highlights.
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// Smith Geometry Function (combined masking-shadowing)
// ---------------------------------------------------
// Accounts for microfacet self-occlusion by reducing specular reflection
// when facets are blocked from either the view direction (V) or the light
// direction (L).
//
// The Smith model assumes masking (view) and shadowing (light) are
// independent effects, so their contributions are multiplied.
// This keeps specular highlights physically plausible, especially at
// grazing angles and higher roughness.
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
// ----------------------------
// Approximates the Fresnel reflectance term, controlling how reflectivity
// increases at grazing view angles. F0 represents base reflectance at
// normal incidence (≈0.04 for dielectrics, albedo for metals).
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================================
// PBR Lighting Calculation
// ----------------------------------------------------------------------------
// Computes physically-based lighting using the Cook–Torrance microfacet BRDF.
// Combines direct lighting from directional and point lights with a small
// ambient term.
//
// The function:
//  - Assumes a 3D camera and evaluates lighting in world space
//  - Uses a simplified view direction (can be replaced with camera position)
//  - Derives F0 from the metallic workflow (dielectric ≈ 0.04, metals use albedo)
//  - Evaluates the Cook–Torrance BRDF per light:
//        BRDF = (D * G * F) / (4 * N·V * N·L)
//  - Enforces energy conservation via kD + kS = 1
//
// Output is outgoing radiance (Lo) accumulated from all lights, plus a
// simple ambient term to approximate indirect / environment lighting.
vec3 CalculatePBRLighting(vec3 albedo, vec3 normal, float metallic, float roughness, float ao) {
    // View direction (currently fixed; can be replaced with normalize(cameraPos - worldPos))
    vec3 V = vec3(0.0, 0.0, 1.0);
    
    // Calculate F0 (reflectance at normal incidence)
    vec3 F0 = vec3(0.04); // Dielectric base reflectivity
    F0 = mix(F0, albedo, metallic);
    
    vec3 Lo = vec3(0.0); // Outgoing radiance
    
    // ========================================================================
    // Directional Light
    // ========================================================================
    if (uHasDirectional > 0) {
        vec3 L = normalize(-uDirLight.Direction);
        vec3 H = normalize(V + L);
        
        vec3 radiance = uDirLight.Color * uDirLight.Intensity;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        float NdotL = max(dot(normal, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // ========================================================================
    // Point Lights
    // ========================================================================
    for (int i = 0; i < uPointLightCount; ++i) {
        GPUPointLight light = uPointLights[i];
        
        // Calculate light direction and distance
        // use all three components!!!
        vec3 lightPos3D = light.PositionAndRange.xyz;
        vec3 L = lightPos3D - vWorldPos;  // vWorldPos is already vec3(x, y, 0.0)
        float distance = length(L);
        L = normalize(L);
        
        // Attenuation
        float range = light.PositionAndRange.w;
        float attenuation = 1.0 - smoothstep(0.0, range, distance);
        if (attenuation <= 0.0) continue;
        
        vec3 H = normalize(V + L);
        vec3 radiance = light.ColorAndIntensity.rgb * light.ColorAndIntensity.a * attenuation;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        float NdotL = max(dot(normal, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // Ambient lighting
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    return ambient + Lo;
}

// ============================================================================
// Main
// ============================================================================
layout (location = 0) out vec4 FragColor;

void main() {
    // Decode flags
    int flags = int(vMaterialFlags);
    bool useNormalMap = (flags & 1) != 0;    // Bit 0
    bool useMetallicMap = (flags & 2) != 0;  // Bit 1
    bool useAOMap = (flags & 4) != 0;        // Bit 2
    bool alphaTest = (flags & 8) != 0;       // Bit 3
    bool unlit = (flags & 16) != 0;          // Bit 4
    bool flatLit = (flags & 32) != 0;        // Bit 5

    // ========================================================================
    // Picking Mode
    // ========================================================================
    if (uPicking > 0) {
        if (vTexIndex >= 0.0) {
            vec4 texColor = texture(uTextures[int(vTexIndex)], vTexCoord);
            if (texColor.a < 0.1)
                discard;
        }
        FragColor = vColor;
        return;
    }

    // ========================================================================
    // Sample Base Color (Albedo)
    // ========================================================================
    vec4 baseColor = vColor;
    if (int(vTexIndex) >= 0) {
        baseColor *= texture(uTextures[int(vTexIndex)], vTexCoord);
    }
    
    // Alpha test (if flag enabled)
    if (alphaTest && baseColor.a < 0.5) discard;
    if (baseColor.a < 0.01) discard;
    
    vec3 albedo = baseColor.rgb;
    
    // ========================================================================
    // Unlit early exit (if flag enabled)
    // ========================================================================
    if (unlit) {
        FragColor = vec4(albedo, baseColor.a);
        return;
    }
    
    // ========================================================================
    // Sample Material2D Properties (using flags)
    // ========================================================================
    bool hasMaterial = (flags != 0 && !unlit);
    
    vec3 normal = DefaultNormal;
    float metallic = vMaterialParams.x;
    float smoothness = vMaterialParams.y;
    float ao = 1.0;

    // flat-lit override (forces constant normal)
    if (flatLit) {
        normal = DefaultNormal; // (0,0,1)
    }

    // Only sample textures if flags say to AND texture exists
    else if (useNormalMap && int(vNormalTexIndex) >= 0) {
        vec3 normalMap = texture(uNormalTextures[int(vNormalTexIndex)], vTexCoord).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        normalMap.xy *= vMaterialParams.w;
        normal = normalize(normalMap);
    }

    // Sample MRA Map (only if flags enabled)
    if (int(vMRATexIndex) >= 0) {
        vec3 mra = texture(uMRATextures[int(vMRATexIndex)], vTexCoord).rgb;
        
        if (useMetallicMap) metallic = mra.r;
        if (useMetallicMap) smoothness = mra.g;
        if (useAOMap) ao = mix(1.0, mra.b, vMaterialParams.z);
    }

    float roughness = 1.0 - smoothness;
    
    // ========================================================================
    // Lighting
    // ========================================================================
    vec3 finalColor = albedo;
    
    if (uLightingEnabled > 0 && hasMaterial) {
        finalColor = CalculatePBRLighting(albedo, normal, metallic, roughness, ao);
    }
    
    // ========================================================================
    // Emissive
    // ========================================================================
    if (int(vEmissiveTexIndex) >= 0 && vEmissiveStrength > 0.0) {
        float emissiveMask = texture(uEmissiveTextures[int(vEmissiveTexIndex)], vTexCoord).r;
        vec3 albedoColor = albedo;
        float luminance = dot(albedoColor, vec3(0.2126, 0.7152, 0.0722));
        if (luminance > 0.001) {
            albedoColor = albedoColor / luminance;
        }
        vec3 emission = albedoColor * emissiveMask * vEmissiveStrength;
        finalColor += emission;
    }
    
    // ========================================================================
    // Output
    // ========================================================================
    FragColor = vec4(finalColor, baseColor.a);
}
