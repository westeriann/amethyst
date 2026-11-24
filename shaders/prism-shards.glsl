// Prism shards: the picture cracks into glass triangles that shift and split colour on every snare.
// fit: calm 0.2 groove 0.6 drop 1.0
float h(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec2 g = uv * vec2(6.0 * iResolution.x / iResolution.y, 6.0);
    vec2 id = floor(g), f = fract(g);
    float tri = step(f.x, f.y);                               // two triangles per square
    float seed = h(id + tri * 7.0 + floor(iBeat));
    float kick = 0.5 * iSnare + 0.8 * iDrop + 0.15 * (1.0 - iBeatPhase);
    vec2 off = (vec2(h(id + seed), h(id - seed)) - 0.5) * 0.04 * kick;
    float split = 0.006 * kick;
    vec3 c;
    c.r = texture(iChannel0, uv + off + vec2(split, 0.0)).r;
    c.g = texture(iChannel0, uv + off).g;
    c.b = texture(iChannel0, uv + off - vec2(split, 0.0)).b;
    float edge = min(min(f.x, f.y), min(1.0 - f.x, 1.0 - f.y));
    edge = min(edge, abs(f.x - f.y) * 0.7);
    c += (1.0 - smoothstep(0.0, 0.03, edge)) * 0.25 * kick;   // glints on the cracks
    fragColor = vec4(c, 1.0);
}
