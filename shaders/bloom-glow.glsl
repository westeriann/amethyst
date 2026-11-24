// Bloom glow: bright parts bleed light, breathing with the bass.
// fit: calm 0.9 groove 0.7 drop 0.6
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 px = 1.0 / iResolution.xy;
    vec3 base = texture(iChannel0, uv).rgb;
    vec3 glow = vec3(0.0);
    float total = 0.0;
    for (int x = -3; x <= 3; x++)
        for (int y = -3; y <= 3; y++) {
            vec2 o = vec2(x, y) * px * 6.0;
            vec3 s = texture(iChannel0, uv + o).rgb;
            float w = exp(-float(x * x + y * y) / 8.0);
            glow += max(s - 0.45, 0.0) * w;   // only the bright parts
            total += w;
        }
    glow /= total;
    float k = 1.5 + 3.0 * iBass + 3.0 * iDrop;
    fragColor = vec4(base + glow * k, 1.0);
}
