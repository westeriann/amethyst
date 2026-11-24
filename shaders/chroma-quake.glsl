// Chroma quake: the colour channels tear apart and the image shakes with the bass. Huge on drops.
// fit: calm 0.1 groove 0.5 drop 1.0
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float kick = pow(1.0 - iBeatPhase, 4.0);
    float amt = 0.004 + 0.02 * iBass * kick + 0.04 * iDrop;
    vec2 shake = vec2(sin(iTime * 91.0), cos(iTime * 77.0)) * amt * 0.5;
    vec2 dir = normalize(uv - 0.5 + 1e-4);
    float r = texture(iChannel0, uv + shake + dir * amt).r;
    float g = texture(iChannel0, uv + shake).g;
    float b = texture(iChannel0, uv + shake - dir * amt).b;
    fragColor = vec4(r, g, b, 1.0);
}
