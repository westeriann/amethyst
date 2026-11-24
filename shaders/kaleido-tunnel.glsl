// Kaleido tunnel: folds the picture into a rotating mirrored tunnel that rushes forward on the beat.
// fit: calm 0.5 groove 0.7 drop 0.8
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 p = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float a = atan(p.y, p.x), r = length(p);
    float seg = 6.2831853 / 8.0;
    a = mod(a + iTime * 0.15, seg);
    a = abs(a - seg * 0.5);                        // mirror inside each slice
    float depth = 0.25 / max(r, 0.02) + iBeat * 0.25 + iDrop * 0.5;
    vec2 uv = vec2(fract(a / seg * 2.0), fract(depth * 0.5));
    uv = abs(uv * 2.0 - 1.0);                      // keep it seamless
    vec3 c = texture(iChannel0, uv).rgb;
    c *= smoothstep(0.0, 0.35, r);                 // dark centre = depth
    fragColor = vec4(c, 1.0);
}
