// Liquid warp: the picture flows like it is seen through moving water. Slow and smooth in quiet parts.
// fit: calm 0.9 groove 0.7 drop 0.3
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float t = iTime * (0.15 + 0.25 * iLevel);
    vec2 q = uv * 3.0;
    vec2 w = vec2(noise(q + t), noise(q + vec2(5.2, 1.3) - t)) - 0.5;
    w += 0.5 * (vec2(noise(q * 2.0 - t * 1.7), noise(q * 2.0 + vec2(8.3, 2.8) + t)) - 0.5);
    float amt = 0.012 + 0.02 * iMid + 0.01 * iHat;
    vec3 c = texture(iChannel0, uv + w * amt).rgb;
    fragColor = vec4(c, 1.0);
}
