#version 460 core
layout (location = 0) out vec4 FragColor;

in vec2 vUV;

uniform sampler2D uOccluder;
uniform vec2  uLightPosNDC;
uniform int   uSamples;
uniform float uDecay;
uniform float uDensity;
uniform float uWeight;
uniform float uTime;
uniform vec2  uCameraWorldPos;

float hash(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash(i),             hash(i + vec2(1,0)), f.x),
        mix(hash(i + vec2(0,1)), hash(i + vec2(1,1)), f.x),
        f.y
    );
}

float fbm(vec2 p) {
    float v = 0.0;
    v += 0.600 * noise(p);
    v += 0.400 * noise(p * 2.1 + vec2(1.7, 9.2));
    return v;
}

void main()
{
    vec2 uv  = vUV;
    vec2 dir = (uv - uLightPosNDC) * (1.0 / float(uSamples)) * uDensity;

    float illumination = 1.0;
    vec3  accumulated  = vec3(0.0);

    for (int i = 0; i < uSamples; i++)
    {
        uv -= dir;
        vec3 s = texture(uOccluder, uv).rgb;
        accumulated += s * illumination * uWeight;
        illumination *= uDecay;
    }

    vec2 shaftDir = normalize(vUV - uLightPosNDC);

    // World-space anchor: offset noise by camera position
    // so the pattern stays fixed to the world, not the screen.
    // Adjust 0.001 to match your world unit scale —
    // larger = noise moves more per world unit (smaller effective tile size)
    vec2 worldOffset = uCameraWorldPos * 0.001;

    vec2 noiseUV = vUV * 3.0
                 + worldOffset
                 + shaftDir * uTime * 0.04
                 + vec2(uTime * 0.01, 0.0);

    float shaftMask = fbm(noiseUV);
    shaftMask       = pow(shaftMask, 1.5);
    shaftMask       = 0.4 + 0.6 * shaftMask;

    accumulated *= shaftMask;

    FragColor = vec4(accumulated, 1.0);
}