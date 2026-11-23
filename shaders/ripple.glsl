// Ripple: a water ring leaves the centre on every beat; bigger on the bass and on drops.
// Custom Amethyst effect. Inputs: iChannel0 (picture), iChannel1 (last output), iResolution, iTime,
// iBeat, iBeatPhase (0..1 inside the beat), iBass, iMid, iTreble, iLevel, iDrop (1 on a drop, fading), iVocal.
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 p = uv - 0.5;
    p.x *= iResolution.x / iResolution.y;
    float r = length(p);
    float front = iBeatPhase * 0.9;                      // ring radius grows through the beat
    float ring = exp(-pow((r - front) * 18.0, 2.0));
    float strength = (0.012 + 0.03 * iBass + 0.05 * iDrop) * (1.0 - iBeatPhase);
    vec2 dir = r > 0.0001 ? p / r : vec2(0.0);
    vec2 off = dir * ring * strength;
    off.x /= iResolution.x / iResolution.y;
    vec3 c = texture(iChannel0, uv - off).rgb;
    c += ring * (0.15 + 0.4 * iDrop) * (1.0 - iBeatPhase);  // a faint bright edge
    fragColor = vec4(c, 1.0);
}
