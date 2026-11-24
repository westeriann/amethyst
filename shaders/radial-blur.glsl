// Radial blur: a zoom-burst from the centre that slams on the kick and on drops.
// fit: calm 0.2 groove 0.6 drop 1.0
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 d = uv - 0.5;
    float amt = 0.02 + 0.06 * iBass * (1.0 - iBeatPhase) + 0.12 * iDrop;
    vec3 c = vec3(0.0);
    float wsum = 0.0;
    for (int i = 0; i < 12; i++) {
        float k = float(i) / 11.0;
        float w = 1.0 - k * 0.7;
        c += texture(iChannel0, 0.5 + d * (1.0 - amt * k)).rgb * w;
        wsum += w;
    }
    fragColor = vec4(c / wsum * (1.0 + 0.2 * iDrop), 1.0);
}
