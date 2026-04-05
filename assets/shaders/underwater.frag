#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uNoiseMap;

uniform float uTime;
uniform vec2  uResolution;

// Layer A — slow drift
uniform float uDriftStrength;
uniform float uDriftSpeed;
uniform float uDriftScale;

// Layer B — ripple
uniform float uRippleStrength;
uniform float uRippleSpeed;
uniform float uRippleScale;

void main()
{
    // =========================
    // Layer A: slow drift
    // =========================
    vec2 driftDir = vec2(0.7, 0.3);
    vec2 driftUV = vUV * uDriftScale + uTime * uDriftSpeed * driftDir;

    float driftX = texture(uNoiseMap, driftUV).r * 2.0 - 1.0;
    float driftY = texture(uNoiseMap, driftUV + vec2(0.5, 0.37)).r * 2.0 - 1.0;

    vec2 driftOffset = vec2(driftX, driftY) * uDriftStrength;

    // =========================
    // Layer B: ripple
    // =========================
    vec2 rippleDir = vec2(-0.4, 0.6);
    vec2 rippleUV = vUV * uRippleScale + uTime * uRippleSpeed * rippleDir;

    float rippleX = texture(uNoiseMap, rippleUV).r * 2.0 - 1.0;
    float rippleY = texture(uNoiseMap, rippleUV + vec2(0.33, 0.71)).r * 2.0 - 1.0;

    vec2 rippleOffset = vec2(rippleX, rippleY) * uRippleStrength;

    // =========================
    // Combine offsets
    // =========================
    vec2 offset = driftOffset + rippleOffset;

    // Convert pixel displacement → UV space

    vec2 displacedUV = vUV + offset;

    // Clamp to avoid sampling outside texture
    displacedUV = clamp(displacedUV, vec2(0.001), vec2(0.999));

    FragColor = texture(uScene, displacedUV);
}