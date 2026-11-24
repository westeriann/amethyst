// Halftone pop: the picture becomes a comic-book dot screen; dots swell on every beat.
// fit: calm 0.3 groove 0.8 drop 0.7
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float cell = mix(7.0, 12.0, iBass) * iResolution.y / 720.0;
    mat2 rot = mat2(0.866, -0.5, 0.5, 0.866);              // 30 degree screen like print
    vec2 p = rot * fragCoord / cell;
    vec2 c = (inverse(rot) * (floor(p) + 0.5)) * cell;     // centre of this dot, back in pixels
    vec3 col = texture(iChannel0, c / iResolution.xy).rgb;
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    float pump = 1.0 + 0.35 * (1.0 - iBeatPhase) + 0.4 * iDrop;
    float r = sqrt(lum) * 0.62 * pump;
    float d = length(fract(p) - 0.5);
    float dotMask = 1.0 - smoothstep(r - 0.06, r + 0.06, d);
    fragColor = vec4(col * dotMask * 1.25, 1.0);
}
