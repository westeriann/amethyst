// Edge neon: outlines everything in glowing neon lines whose colour drifts with the beat; trails via iChannel1.
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 px = 1.5 / iResolution.xy;
    float l[9];
    int k = 0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) l[k++] = dot(texture(iChannel0, uv + vec2(x, y) * px).rgb, vec3(0.333));
    float gx = l[2] + 2.0 * l[5] + l[8] - l[0] - 2.0 * l[3] - l[6];
    float gy = l[6] + 2.0 * l[7] + l[8] - l[0] - 2.0 * l[1] - l[2];
    float e = clamp(length(vec2(gx, gy)) * (2.0 + 4.0 * iBass), 0.0, 1.0);
    vec3 hue = 0.5 + 0.5 * cos(6.2831853 * (iBeat * 0.0625 + vec3(0.0, 0.33, 0.67)));
    vec3 trail = texture(iChannel1, uv).rgb * 0.82;
    fragColor = vec4(max(hue * e * (1.0 + iDrop), trail), 1.0);
}
