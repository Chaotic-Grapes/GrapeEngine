/* Start Header *****************************************************************/
/*!
\file   boid.frag
\author Choi Meng Yew (100%)
\date   13th March 2026
\brief
Physically Based Rendering (PBR) fragment shader used for instanced
boid rendering in GrapeEngine.

This shader performs lighting using a Cook–Torrance BRDF with GGX
distribution and supports both directional and point lights supplied
via a GPU SSBO. Material properties can be sourced from textures or
uniform parameters, enabling flexible rendering for boid instances.

Features:
    - Cook–Torrance PBR lighting model (GGX + Schlick Fresnel)
    - Directional and point light support
    - GPU point lights stored in SSBO
    - Albedo texture support
    - Normal mapping
    - Metallic / Roughness / AO (MRA) map support
    - Emissive map support
    - Alpha testing and transparency
    - Editor picking mode

Responsibilities:
    - Decode material flags and determine shading path
    - Sample material textures (Albedo, Normal, MRA, Emissive)
    - Compute PBR lighting contributions per light
    - Apply emissive contribution
    - Output final shaded fragment color

Used by:
    - BoidSystem instanced renderer
    - GrapeEngine material pipeline
    - GPU point-light lighting system

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#version 460 core

in vec2 vTexCoord;
in vec3 vWorldPos;

// ============================================================================
// UNIFORMS — set once per flock draw call by DrawFlocksByLayer
// ============================================================================
uniform int   uPicking;
uniform int   uLightingEnabled;

uniform sampler2D uTexture;
uniform int       uHasTexture;
uniform vec4      uColor;

uniform sampler2D uEmissiveTex;
uniform int       uHasEmissive;
uniform float     uEmissiveStrength;

uniform sampler2D uNormalMap;
uniform int       uHasNormalMap;

uniform sampler2D uMRAMap;
uniform int       uHasMRAMap;

uniform float uMetallic;
uniform float uSmoothness;
uniform float uAOStrength;
uniform float uNormalStrength;
uniform int   uMaterialFlags;

// ============================================================================
// Light data — same SSBO binding as batch.frag
// ============================================================================
struct GPUPointLight {
    vec4 PositionAndRange;
    vec4 ColorAndIntensity;
};

layout(std430, binding = 0) buffer PointLightBuffer {
    GPUPointLight uPointLights[];
};

struct DirLight {
    vec3  Direction;
    vec3  Color;
    float Intensity;
};

uniform int     uPointLightCount;
uniform int     uHasDirectional;
uniform DirLight uDirLight;

// ============================================================================
// PBR Helper Functions (identical to batch.frag)
// ============================================================================
const float PI = 3.14159265359;
const vec3  DefaultNormal = vec3(0.0, 0.0, 1.0);

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculatePBRLighting(vec3 albedo, vec3 normal, float metallic, float roughness, float ao) {
    vec3 V  = vec3(0.0, 0.0, 1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // Directional light
    if (uHasDirectional > 0) {
        vec3  L        = normalize(-uDirLight.Direction);
        vec3  H        = normalize(V + L);
        vec3  radiance = uDirLight.Color * uDirLight.Intensity;
        float NDF      = DistributionGGX(normal, H, roughness);
        float G        = GeometrySmith(normal, V, L, roughness);
        vec3  F        = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3  specular = (NDF * G * F) / (4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001);
        vec3  kD       = (vec3(1.0) - F) * (1.0 - metallic);
        Lo += (kD * albedo / PI + specular) * radiance * max(dot(normal, L), 0.0);
    }

    // Point lights
    for (int i = 0; i < uPointLightCount; ++i) {
        GPUPointLight light = uPointLights[i];
        vec3  L           = light.PositionAndRange.xyz - vWorldPos;
        float distance    = length(L);
        L                 = normalize(L);
        float range       = light.PositionAndRange.w;
        float attenuation = 1.0 - smoothstep(0.0, range, distance);
        if (attenuation <= 0.0) continue;
        vec3  H        = normalize(V + L);
        vec3  radiance = light.ColorAndIntensity.rgb * light.ColorAndIntensity.a * attenuation;
        float NDF      = DistributionGGX(normal, H, roughness);
        float G        = GeometrySmith(normal, V, L, roughness);
        vec3  F        = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3  specular = (NDF * G * F) / (4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001);
        vec3  kD       = (vec3(1.0) - F) * (1.0 - metallic);
        Lo += (kD * albedo / PI + specular) * radiance * max(dot(normal, L), 0.0);
    }

    return vec3(0.03) * albedo * ao + Lo;
}

// ============================================================================
// Main
// ============================================================================
layout(location = 0) out vec4 FragColor;

void main() {
    // Decode material flags (same bit layout as batch.frag)
    int  flags         = uMaterialFlags;
    bool useNormalMap  = (flags & 1)  != 0;
    bool useMetallic   = (flags & 2)  != 0;
    bool useAOMap      = (flags & 4)  != 0;
    bool alphaTest     = (flags & 8)  != 0;
    bool unlit         = (flags & 16) != 0;
    bool flatLit       = (flags & 32) != 0;

    // ========================================================================
    // Picking
    // ========================================================================
    if (uPicking > 0) {
        if (uHasTexture == 1) {
            vec4 texColor = texture(uTexture, vTexCoord);
            if (texColor.a < 0.1) discard;
        }
        FragColor = uColor;
        return;
    }

    // ========================================================================
    // Albedo
    // ========================================================================
    vec4 baseColor = uColor;
    if (uHasTexture == 1)
        baseColor *= texture(uTexture, vTexCoord);

    if (alphaTest && baseColor.a < 0.5) discard;
    if (baseColor.a < 0.01)             discard;

    vec3 albedo = baseColor.rgb;

    // ========================================================================
    // Unlit early exit
    // ========================================================================
    if (unlit) {
        FragColor = vec4(albedo, baseColor.a);
        return;
    }

    // ========================================================================
    // Normal
    // ========================================================================
    vec3 normal = DefaultNormal;
    if (!flatLit && useNormalMap && uHasNormalMap == 1) {
        vec3 n  = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        n.xy   *= uNormalStrength;
        normal  = normalize(n);
    }

    // ========================================================================
    // MRA
    // ========================================================================
    float metallic   = uMetallic;
    float smoothness = uSmoothness;
    float ao         = 1.0;

    if (uHasMRAMap == 1) {
        vec3 mra = texture(uMRAMap, vTexCoord).rgb;
        if (useMetallic) metallic   = mra.r;
        if (useMetallic) smoothness = mra.g;
        if (useAOMap)    ao         = mix(1.0, mra.b, uAOStrength);
    }

    float roughness = 1.0 - smoothness;

    // ========================================================================
    // Lighting
    // ========================================================================
    vec3 finalColor = albedo;
    bool hasMaterial = (flags != 0 && !unlit);

    if (uLightingEnabled > 0 && hasMaterial)
        finalColor = CalculatePBRLighting(albedo, normal, metallic, roughness, ao);

    // ========================================================================
    // Emissive
    // ========================================================================
    if (uHasEmissive == 1 && uEmissiveStrength > 0.0) {
        float emissiveMask = texture(uEmissiveTex, vTexCoord).r;
        float luminance    = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
        vec3  albedoNorm   = luminance > 0.001 ? albedo / luminance : albedo;
        finalColor        += albedoNorm * emissiveMask * uEmissiveStrength;
    }

    // ========================================================================
    // Output
    // ========================================================================
    FragColor = vec4(finalColor, baseColor.a);
}