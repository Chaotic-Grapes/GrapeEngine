#version 460 core
in vec2  vUV;         // interpolated UV (|vUV| = 1 at circle edge)
in vec4  vColor;      // base color
in float vStrokePx;   // stroke thickness in pixels
out vec4 FragColor;

void main() {
    // Signed distance from current pixel to the circle boundary
    float d = length(vUV) - 1.0;

    // Estimate edge width in screen space for smooth anti-aliased transition
    float aa = 0.5 * fwidth(d);

    float alpha;
    if (vStrokePx <= 0.0) {
        // ---- Filled circle ----
        // Fade alpha smoothly inside the unit circle boundary
        alpha = smoothstep(+aa, -aa, d);
    } else {
        // ---- Stroked circle ----
        // Convert stroke thickness (pixels) into UV-space thickness
        float k    = length(fwidth(vUV));           // UV/pixel scale factor
        float t_uv = max(vStrokePx * k, 1e-6);      // UV thickness of stroke band
        float band = abs(d) - 0.5 * t_uv;           // distance from stroke region
        alpha = 1.0 - smoothstep(-aa, +aa, band);   // smooth edges on both sides
    }

    // Apply color and alpha blending
    vec4 col = vec4(vColor.rgb, vColor.a * alpha);

    // Discard near-zero alpha fragments to reduce banding artifacts
    if (col.a <= 0.001) discard;

    FragColor = col;
}
