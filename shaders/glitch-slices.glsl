// Glitch slices: horizontal strips jump sideways and split colour on snares and drops; clean in between.
// fit: calm 0.1 groove 0.5 drop 0.9
float h(float x) { return fract(sin(x * 91.7) * 43758.5453); }
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float hit = max(iSnare, iDrop);
    float rows = 24.0;
    float row = floor(uv.y * rows);
    float t = floor(iTime * 20.0);
    float r = h(row + t);
    float shift = r > 1.0 - 0.45 * hit ? (h(row * 3.1 + t) - 0.5) * 0.12 * hit : 0.0;
    float split = 0.008 * hit;
    vec3 c;
    c.r = texture(iChannel0, uv + vec2(shift + split, 0.0)).r;
    c.g = texture(iChannel0, uv + vec2(shift, 0.0)).g;
    c.b = texture(iChannel0, uv + vec2(shift - split, 0.0)).b;
    fragColor = vec4(c, 1.0);
}
