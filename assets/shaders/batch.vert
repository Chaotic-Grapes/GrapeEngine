#version 460 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTexIndex;
layout(location = 5) in float aEmissiveTexIndex;   // (skip 4, that's strokePx in other shader)
layout(location = 6) in float aEmissiveStrength;
layout(location = 7) in float aNormalTexIndex;
layout(location = 8) in float aMRATexIndex;
layout(location = 9) in vec4 aMaterialParams;
layout(location = 10) in float aMaterialFlags;

uniform mat4 uViewProj;

out vec2 vTexCoord;
out vec4 vColor;
flat out float vTexIndex;

flat out float vEmissiveTexIndex;
out float vEmissiveStrength;

flat out float vNormalTexIndex;
flat out float vMRATexIndex;
out vec4 vMaterialParams; 
flat out float vMaterialFlags;

// Pass world position to fragment shader for point-light distance
// In the pipeline, aPos is already in world space (we batch CPU-side).
out vec3 vWorldPos;

void main() {
    // Existing transform
    gl_Position = uViewProj * vec4(aPos.xy, 0.0, 1.0);

    // Existing varyings
    vTexCoord           = aTexCoord;
    vColor              = aColor;
    vTexIndex           = aTexIndex;
    vEmissiveTexIndex   = aEmissiveTexIndex;
    vEmissiveStrength   = aEmissiveStrength;
    vNormalTexIndex     = aNormalTexIndex;
    vMRATexIndex        = aMRATexIndex;
    vMaterialParams     = aMaterialParams;
    vMaterialFlags      = aMaterialFlags;
    
    // World position (2D -> z = 0)
    vWorldPos = vec3(aPos.xy, 0.0);
}
