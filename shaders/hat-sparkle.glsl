// Hat sparkle: tiny stars twinkle across the bright parts of the picture on every hi-hat.
// fit: calm 0.8 groove 0.8 drop 0.4
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec3 c = texture(iChannel0, uv).rgb;
    float cell = 14.0 * iResolution.y / 720.0;
    vec2 id = floor(fragCoord / cell), f = fract(fragCoord / cell) - 0.5;
    float pick = h(id + floor(iTime * 8.0));                  // a new set of stars every 1/8 s
    float lum = dot(texture(iChannel0, (id + 0.5) * cell / iResolution.xy).rgb, vec3(0.3, 0.59, 0.11));
    float on = step(0.93 - 0.05 * iHat, pick) * smoothstep(0.15, 0.6, lum) * (0.35 + iHat);
    float star = max(0.0, 1.0 - length(f) * 3.2) + max(0.0, 1.0 - abs(f.x) * 14.0) * max(0.0, 1.0 - abs(f.y) * 2.6)
               + max(0.0, 1.0 - abs(f.y) * 14.0) * max(0.0, 1.0 - abs(f.x) * 2.6);
    c += vec3(1.0, 0.95, 0.9) * star * on * 0.8;
    fragColor = vec4(c, 1.0);
}
