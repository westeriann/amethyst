#include "gfx.h"

#include <GL/glew.h>

#include <cmath>
#include <cstdio>

namespace {

const char* kVs = R"(#version 330 core
out vec2 uv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    uv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";

const char* kMixFs = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D a, b; uniform float x; uniform int mode;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
    vec3 A = texture(a, uv).rgb, B = texture(b, uv).rgb;
    float wa = min(1.0, 2.0 * (1.0 - x)), wb = min(1.0, 2.0 * x);   // both full at the centre
    vec3 c;
    if (mode == 0) c = mix(A, B, x);
    else if (mode == 1) c = A * wa + B * wb;
    else if (mode == 2) c = 1.0 - (1.0 - A * wa) * (1.0 - B * wb);
    else if (mode == 3) c = mix(mix(A, A * B, min(1.0, 2.0 * x)), B, max(0.0, 2.0 * x - 1.0));
    else if (mode == 4) c = abs(A * wa - B * wb);
    else if (mode == 5) c = max(A * wa, B * wb);
    else { float k = smoothstep(0.08, 0.35, luma(B)) * min(1.0, 2.0 * x); c = mix(A * wa, B, k); }
    o = vec4(clamp(c, 0.0, 1.0), 1.0);
})";

const char* kTransFs = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D from, to; uniform float p, time; uniform int type; uniform vec2 res;
float rnd(vec2 q) { return fract(sin(dot(q, vec2(12.9898, 78.233))) * 43758.5453); }
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
    vec3 F = texture(from, uv).rgb, T = texture(to, uv).rgb, c;
    float aspect = res.x / res.y;
    if (type == 1) {                       // wipe left -> right with a soft edge
        float e = p * 1.2 - 0.1;
        c = mix(T, F, smoothstep(e - 0.05, e + 0.05, uv.x));
    } else if (type == 2) {                // iris opens from the centre
        float r = length((uv - 0.5) * vec2(aspect, 1.0));
        float e = p * 1.1;
        c = mix(T, F, smoothstep(e - 0.03, e + 0.03, r));
    } else if (type == 3) {                // zoom through the old image into the new one
        vec3 Fz = texture(from, (uv - 0.5) / (1.0 + p * 4.0) + 0.5).rgb;
        vec3 Tz = texture(to, (uv - 0.5) * (1.0 + (1.0 - p) * 1.5) + 0.5).rgb;
        c = mix(Fz, Tz, smoothstep(0.35, 0.75, p));
    } else if (type == 4) {                // glitchy block swap
        vec2 cell = floor(uv * vec2(16.0, 9.0));
        float r = rnd(cell + floor(time * 14.0) * 0.37);
        float jitter = (rnd(vec2(floor(uv.y * 40.0), floor(time * 30.0))) - 0.5) * 0.06 * sin(p * 3.14159);
        vec3 Fj = texture(from, uv + vec2(jitter, 0.0)).rgb, Tj = texture(to, uv - vec2(jitter, 0.0)).rgb;
        c = r < p ? Tj : Fj;
    } else if (type == 5) {                // pixel dissolve
        float n = rnd(floor(uv * res / 3.0));
        c = n < p ? T : F;
    } else if (type == 6) {                // dark areas of the old image give way first
        float k = smoothstep(p * 1.2 - 0.2, p * 1.2, luma(F));
        c = mix(T, F, k);
    } else if (type == 7) {                // slide the new image in from the right
        float edge = 1.0 - p;
        c = uv.x < edge ? texture(from, uv + vec2(p, 0.0)).rgb : texture(to, uv - vec2(edge, 0.0)).rgb;
    } else c = mix(F, T, p);
    o = vec4(c, 1.0);
})";

const char* kFxFs = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D src, prev;
uniform vec2 res; uniform float time;
uniform float kaleido, zoom, rot, swirl, pixel, split, hue, sat, contrast, bright, invert;
uniform int mirror;
uniform float fb, fbZoom, fbRot;
vec2 rotate(vec2 p, float a) { float c = cos(a), s = sin(a); return vec2(c * p.x - s * p.y, s * p.x + c * p.y); }
vec3 hueShift(vec3 c, float h) {
    const mat3 toYIQ = mat3(0.299, 0.596, 0.211, 0.587, -0.274, -0.523, 0.114, -0.322, 0.312);
    const mat3 toRGB = mat3(1.0, 1.0, 1.0, 0.956, -0.272, -1.106, 0.621, -0.647, 1.703);
    vec3 yiq = toYIQ * c;
    float a = h * 6.2831853;
    yiq.yz = rotate(yiq.yz, a);
    return toRGB * yiq;
}
void main() {
    float aspect = res.x / res.y;
    vec2 p = uv - 0.5;
    p.x *= aspect;
    p = rotate(p, rot) / max(zoom, 0.05);
    if (swirl != 0.0) { float r = length(p); p = rotate(p, swirl * max(0.0, 0.7 - r) * 3.0); }
    if (kaleido >= 2.0) {
        float r = length(p), a = atan(p.y, p.x);
        float seg = 6.2831853 / kaleido;
        a = mod(a, seg);
        a = abs(a - seg * 0.5);
        p = vec2(cos(a), sin(a)) * r;
    }
    if (mirror == 1 || mirror == 3) p.x = -abs(p.x);
    if (mirror == 2 || mirror == 3) p.y = -abs(p.y);
    p.x /= aspect;
    vec2 q = p + 0.5;
    q = abs(mod(q + 1.0, 2.0) - 1.0);   // mirrored repeat keeps zoomed-out edges filled
    if (pixel > 1.0) { vec2 cell = pixel / res; q = (floor(q / cell) + 0.5) * cell; }
    vec3 c;
    if (split > 0.0) {
        vec2 d = (q - 0.5) * split * 2.0 + vec2(split, 0.0);
        c = vec3(texture(src, q + d).r, texture(src, q).g, texture(src, q - d).b);
    } else c = texture(src, q).rgb;
    if (hue != 0.0) c = hueShift(c, hue);
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, sat);
    c = (c - 0.5) * contrast + 0.5 + bright;
    if (invert > 0.5) c = 1.0 - c;
    c = clamp(c, 0.0, 1.0);
    if (fb > 0.0) {
        vec2 f = uv - 0.5;
        f.x *= aspect;
        f = rotate(f, fbRot) / max(fbZoom, 0.05);
        f.x /= aspect;
        vec3 pv = texture(prev, f + 0.5).rgb * fb;
        c = max(c, pv);
    }
    o = vec4(c, 1.0);
})";

const char* kLayerVs = R"(#version 330 core
uniform vec4 rect; uniform float flipY;
out vec2 uv;
void main() {
    vec2 t = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
    uv = vec2(t.x, mix(t.y, 1.0 - t.y, flipY));
    gl_Position = vec4(mix(rect.xy, rect.zw, t), 0.0, 1.0);
})";
const char* kLayerFs = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D tex; uniform float opacity;
void main() { vec4 c = texture(tex, uv); o = vec4(c.rgb, c.a * opacity); }
)";

const char* kFinalFs = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D src; uniform float black, white;
void main() {
    vec3 c = texture(src, uv).rgb * (1.0 - black);
    o = vec4(mix(c, vec3(1.0), white), 1.0);
})";

const char* kPresentFs = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D src; uniform mat3 hinv; uniform int grid; uniform float flipY;
void main() {
    vec3 h = hinv * vec3(uv, 1.0);
    vec2 t = h.xy / h.z;
    if (h.z <= 0.0 || t.x < 0.0 || t.y < 0.0 || t.x > 1.0 || t.y > 1.0) { o = vec4(0.0, 0.0, 0.0, 1.0); return; }
    vec2 s = vec2(t.x, mix(t.y, 1.0 - t.y, flipY));
    vec3 c = texture(src, s).rgb;
    if (grid == 1) {
        vec2 g = abs(fract(t * 8.0 + 0.5) - 0.5) / fwidth(t * 8.0);
        float line = 1.0 - min(min(g.x, g.y), 1.0);
        float edge = step(min(min(t.x, t.y), min(1.0 - t.x, 1.0 - t.y)), 0.006);
        vec2 d = (t - 0.5); float circ = 1.0 - min(abs(length(d) - 0.35) / fwidth(length(d)) / 1.5, 1.0);
        c = mix(c * 0.35, vec3(1.0), max(max(line, edge), circ));
        if (t.x < 0.06 && t.y > 0.94) c = vec3(1.0, 0.3, 0.4);   // top-left marker
    }
    o = vec4(c, 1.0);
})";

unsigned compileProgram(const char* vs, const char* fs, const char* name) {
    auto sh = [&](GLenum t, const char* s) {
        GLuint h = glCreateShader(t);
        glShaderSource(h, 1, &s, nullptr);
        glCompileShader(h);
        GLint ok = 0;
        glGetShaderiv(h, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(h, sizeof log, nullptr, log);
            fprintf(stderr, "[gfx] %s shader: %s\n", name, log);
        }
        return h;
    };
    GLuint p = glCreateProgram();
    glAttachShader(p, sh(GL_VERTEX_SHADER, vs));
    glAttachShader(p, sh(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    return p;
}

const char* kUserHeader = R"(#version 330 core
in vec2 uv;
out vec4 amethystOut;
uniform sampler2D iChannel0;  // the picture so far
uniform sampler2D iChannel1;  // this effect's previous output (feedback)
uniform vec3 iResolution;
uniform float iTime, iBeat, iBeatPhase, iBass, iMid, iTreble, iLevel, iAmount, iDrop, iVocal;
uniform int iFrame;
)";
const char* kUserFooter = R"(
void main() {
    vec4 c = vec4(0.0);
    mainImage(c, uv * iResolution.xy);
    amethystOut = vec4(mix(texture(iChannel0, uv).rgb, c.rgb, clamp(iAmount, 0.0, 1.0)), 1.0);
}
)";

void bindTex(int unit, unsigned tex) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex);
}

// Homography mapping the unit square to quad q (q0=(0,0) q1=(1,0) q2=(1,1) q3=(0,1)); returns row-major 3x3.
void squareToQuad(const float q[8], double m[9]) {
    double x0 = q[0], y0 = q[1], x1 = q[2], y1 = q[3], x2 = q[4], y2 = q[5], x3 = q[6], y3 = q[7];
    double dx1 = x1 - x2, dx2 = x3 - x2, dx3 = x0 - x1 + x2 - x3;
    double dy1 = y1 - y2, dy2 = y3 - y2, dy3 = y0 - y1 + y2 - y3;
    double g = 0, h = 0;
    if (std::fabs(dx3) > 1e-12 || std::fabs(dy3) > 1e-12) {
        double den = dx1 * dy2 - dx2 * dy1;
        if (std::fabs(den) < 1e-12) den = 1e-12;
        g = (dx3 * dy2 - dx2 * dy3) / den;
        h = (dx1 * dy3 - dx3 * dy1) / den;
    }
    m[0] = x1 - x0 + g * x1; m[1] = x3 - x0 + h * x3; m[2] = x0;
    m[3] = y1 - y0 + g * y1; m[4] = y3 - y0 + h * y3; m[5] = y0;
    m[6] = g;                m[7] = h;                m[8] = 1;
}

void invert3(const double m[9], float out[9]) {
    double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    double A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    double det = a * A + b * B + c * C;
    if (std::fabs(det) < 1e-12) det = 1e-12;
    double inv[9] = {A, -(b * i - c * h), b * f - c * e, B, a * i - c * g, -(a * f - c * d), C, -(a * h - b * g), a * e - b * d};
    for (int k = 0; k < 9; k++) out[k] = (float)(inv[k] / det);
}

}  // namespace

bool RenderTarget::ensure(int width, int height) {
    if (width == w && height == h && fbo) return false;
    w = width;
    h = height;
    if (!tex) glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (!fbo) glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

bool Gfx::init() {
    glGenVertexArrays(1, &vao_);
    mixP_ = compileProgram(kVs, kMixFs, "mix");
    transP_ = compileProgram(kVs, kTransFs, "transition");
    fxP_ = compileProgram(kVs, kFxFs, "fx");
    layerP_ = compileProgram(kLayerVs, kLayerFs, "layer");
    finalP_ = compileProgram(kVs, kFinalFs, "final");
    presentP_ = compileProgram(kVs, kPresentFs, "present");
    return mixP_ && fxP_ && layerP_ && finalP_ && presentP_;
}

void Gfx::mix(RenderTarget& out, unsigned a, unsigned b, float x, int mode) {
    out.bind();
    glDisable(GL_BLEND);
    glUseProgram(mixP_);
    glBindVertexArray(vao_);
    bindTex(0, a);
    bindTex(1, b);
    glUniform1i(glGetUniformLocation(mixP_, "a"), 0);
    glUniform1i(glGetUniformLocation(mixP_, "b"), 1);
    glUniform1f(glGetUniformLocation(mixP_, "x"), x);
    glUniform1i(glGetUniformLocation(mixP_, "mode"), mode);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Gfx::transition(RenderTarget& out, unsigned from, unsigned to, float progress, int type, float time) {
    out.bind();
    glDisable(GL_BLEND);
    glUseProgram(transP_);
    glBindVertexArray(vao_);
    bindTex(0, from);
    bindTex(1, to);
    glUniform1i(glGetUniformLocation(transP_, "from"), 0);
    glUniform1i(glGetUniformLocation(transP_, "to"), 1);
    glUniform1f(glGetUniformLocation(transP_, "p"), progress);
    glUniform1f(glGetUniformLocation(transP_, "time"), time);
    glUniform1i(glGetUniformLocation(transP_, "type"), type);
    glUniform2f(glGetUniformLocation(transP_, "res"), (float)out.w, (float)out.h);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Gfx::fx(RenderTarget& out, unsigned src, unsigned prev, const FxParams& p, float rotation, float time) {
    out.bind();
    glDisable(GL_BLEND);
    glUseProgram(fxP_);
    glBindVertexArray(vao_);
    bindTex(0, src);
    bindTex(1, prev);
    auto u = [&](const char* n) { return glGetUniformLocation(fxP_, n); };
    glUniform1i(u("src"), 0);
    glUniform1i(u("prev"), 1);
    glUniform2f(u("res"), (float)out.w, (float)out.h);
    glUniform1f(u("time"), time);
    glUniform1f(u("kaleido"), p.kaleido ? (float)p.kaleidoSegments : 0.f);
    glUniform1i(u("mirror"), p.mirror ? p.mirrorMode : 0);
    glUniform1f(u("zoom"), p.zoom);
    glUniform1f(u("rot"), rotation);
    glUniform1f(u("swirl"), p.swirl);
    glUniform1f(u("pixel"), p.pixelate ? p.pixelSize : 0.f);
    glUniform1f(u("split"), p.rgbSplit ? p.rgbSplitAmount : 0.f);
    glUniform1f(u("hue"), p.hue);
    glUniform1f(u("sat"), p.saturation);
    glUniform1f(u("contrast"), p.contrast);
    glUniform1f(u("bright"), p.brightness);
    glUniform1f(u("invert"), p.invert ? 1.f : 0.f);
    glUniform1f(u("fb"), p.feedback ? p.feedbackAmount : 0.f);
    glUniform1f(u("fbZoom"), p.feedbackZoom);
    glUniform1f(u("fbRot"), p.feedbackRotate);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Gfx::layer(unsigned tex, float x0, float y0, float x1, float y1, float opacity, bool flipY, int blend) {
    glEnable(GL_BLEND);
    switch (blend) {
        case BlendAdd: glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
        case BlendScreen: glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE); break;
        case BlendMultiply: glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA); break;
        default: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUseProgram(layerP_);
    glBindVertexArray(vao_);
    bindTex(0, tex);
    glUniform1i(glGetUniformLocation(layerP_, "tex"), 0);
    glUniform4f(glGetUniformLocation(layerP_, "rect"), x0, y0, x1, y1);
    glUniform1f(glGetUniformLocation(layerP_, "flipY"), flipY ? 1.f : 0.f);
    glUniform1f(glGetUniformLocation(layerP_, "opacity"), opacity);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_BLEND);
}

void Gfx::finalize(RenderTarget& out, unsigned src, float black, float white) {
    out.bind();
    glDisable(GL_BLEND);
    glUseProgram(finalP_);
    glBindVertexArray(vao_);
    bindTex(0, src);
    glUniform1i(glGetUniformLocation(finalP_, "src"), 0);
    glUniform1f(glGetUniformLocation(finalP_, "black"), black);
    glUniform1f(glGetUniformLocation(finalP_, "white"), white);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Gfx::present(unsigned tex, int x, int y, int w, int h, const float* c, bool grid, bool flipY) {
    glViewport(x, y, w, h);
    glDisable(GL_BLEND);
    glUseProgram(presentP_);
    glBindVertexArray(vao_);
    bindTex(0, tex);
    float q[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    if (c) {  // UI corners TL,TR,BR,BL (top-left origin) -> GL quad BL,BR,TR,TL (bottom-left origin)
        q[0] = c[6]; q[1] = 1 - c[7];
        q[2] = c[4]; q[3] = 1 - c[5];
        q[4] = c[2]; q[5] = 1 - c[3];
        q[6] = c[0]; q[7] = 1 - c[1];
    }
    double m[9];
    squareToQuad(q, m);
    float inv[9];
    invert3(m, inv);
    glUniform1i(glGetUniformLocation(presentP_, "src"), 0);
    glUniformMatrix3fv(glGetUniformLocation(presentP_, "hinv"), 1, GL_TRUE, inv);
    glUniform1i(glGetUniformLocation(presentP_, "grid"), grid ? 1 : 0);
    glUniform1f(glGetUniformLocation(presentP_, "flipY"), flipY ? 1.f : 0.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Gfx::userFx(RenderTarget& out, unsigned prog, unsigned src, unsigned prev, const UserFxParams& p) {
    out.bind();
    glUseProgram(prog);
    glBindVertexArray(vao_);
    bindTex(0, src);
    bindTex(1, prev);
    glUniform1i(glGetUniformLocation(prog, "iChannel0"), 0);
    glUniform1i(glGetUniformLocation(prog, "iChannel1"), 1);
    glUniform3f(glGetUniformLocation(prog, "iResolution"), (float)out.w, (float)out.h, 1.f);
    glUniform1f(glGetUniformLocation(prog, "iTime"), p.time);
    glUniform1f(glGetUniformLocation(prog, "iBeat"), p.beat);
    glUniform1f(glGetUniformLocation(prog, "iBeatPhase"), p.beatPhase);
    glUniform1f(glGetUniformLocation(prog, "iBass"), p.bass);
    glUniform1f(glGetUniformLocation(prog, "iMid"), p.mid);
    glUniform1f(glGetUniformLocation(prog, "iTreble"), p.treble);
    glUniform1f(glGetUniformLocation(prog, "iLevel"), p.level);
    glUniform1f(glGetUniformLocation(prog, "iAmount"), p.amount);
    glUniform1f(glGetUniformLocation(prog, "iDrop"), p.drop);
    glUniform1f(glGetUniformLocation(prog, "iVocal"), p.vocal);
    glUniform1i(glGetUniformLocation(prog, "iFrame"), p.frame);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

unsigned Gfx::compileUser(const std::string& src, std::string& err) {
    auto sh = [&](GLenum t, const std::string& s, bool& ok) {
        GLuint hnd = glCreateShader(t);
        const char* p = s.c_str();
        glShaderSource(hnd, 1, &p, nullptr);
        glCompileShader(hnd);
        GLint st = 0;
        glGetShaderiv(hnd, GL_COMPILE_STATUS, &st);
        ok = st != 0;
        if (!ok) {
            char log[4096];
            glGetShaderInfoLog(hnd, sizeof log, nullptr, log);
            err = log;
        }
        return hnd;
    };
    bool okV = false, okF = false;
    GLuint vs = sh(GL_VERTEX_SHADER, kVs, okV);
    GLuint fs = sh(GL_FRAGMENT_SHADER, std::string(kUserHeader) + "#line 1\n" + src + kUserFooter, okF);
    if (!okV || !okF) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        err = log;
        glDeleteProgram(p);
        return 0;
    }
    err.clear();
    return p;
}

