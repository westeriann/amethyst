// VHS: scanlines, wobbling tracking, colour bleed and noise. The tape freaks out on drops.
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float glitch = 0.2 + 0.8 * iDrop;
    float line = floor(uv.y * 240.0);
    float jitter = (hash(vec2(line, floor(iTime * 30.0))) - 0.5) * 0.004 * glitch;
    float band = step(0.985 - 0.05 * iDrop, hash(vec2(floor(uv.y * 12.0), floor(iTime * 8.0))));
    uv.x += jitter + band * 0.03 * glitch + sin(uv.y * 20.0 + iTime * 3.0) * 0.0015;
    vec3 c;
    c.r = texture(iChannel0, uv + vec2(0.003, 0.0)).r;
    c.g = texture(iChannel0, uv).g;
    c.b = texture(iChannel0, uv - vec2(0.003, 0.0)).b;
    c *= 0.85 + 0.15 * sin(fragCoord.y * 3.14159);              // scanlines
    c += (hash(fragCoord + iTime) - 0.5) * 0.08 * glitch;       // noise
    c = mix(c, vec3(dot(c, vec3(0.3, 0.59, 0.11))), 0.15);      // washed-out tape colour
    fragColor = vec4(c, 1.0);
}
