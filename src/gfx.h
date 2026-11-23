// GPU compositing: deck mixing, post-effects, overlay layers, final pass and projection-mapped
// presentation (corner pin). All passes are full-screen-triangle shaders on GL 3.3 core.
#pragma once
#include <string>

struct RenderTarget {
    unsigned fbo = 0, tex = 0;
    int w = 0, h = 0;
    bool ensure(int width, int height);  // true when (re)created
    void bind() const;
};

enum MixMode { MixCrossfade, MixAdd, MixScreen, MixMultiply, MixDifference, MixLighten, MixLumaKey, MixModeCount };
inline const char* mixModeName(int m) {
    static const char* n[] = {"Crossfade", "Add", "Screen", "Multiply", "Difference", "Lighten", "Luma key"};
    return m >= 0 && m < MixModeCount ? n[m] : "?";
}

struct FxParams {
    bool kaleido = false;  int kaleidoSegments = 6;
    bool mirror = false;   int mirrorMode = 3;  // 1 horizontal, 2 vertical, 3 quad
    bool feedback = false; float feedbackAmount = 0.85f, feedbackZoom = 1.02f, feedbackRotate = 0.0f;
    bool rgbSplit = false; float rgbSplitAmount = 0.01f;
    bool pixelate = false; float pixelSize = 12;
    bool invert = false;
    bool strobe = false;   float strobeRate = 8; bool strobeOnBeat = true;
    float zoom = 1, rotateSpeed = 0, swirl = 0;
    float hue = 0, saturation = 1, contrast = 1, brightness = 0;
};

enum Transition { TransBlend, TransWipe, TransIris, TransZoom, TransGlitch, TransDissolve, TransLuma, TransSlide, TransitionCount };
inline const char* transitionName(int t) {
    static const char* n[] = {"Blend (mix mode)", "Wipe", "Iris", "Zoom through", "Glitch blocks", "Dissolve", "Luma fade", "Slide"};
    return t >= 0 && t < TransitionCount ? n[t] : "?";
}

enum BlendMode { BlendNormal, BlendAdd, BlendScreen, BlendMultiply, BlendModeCount };
inline const char* blendModeName(int m) {
    static const char* n[] = {"Normal", "Add", "Screen", "Multiply"};
    return m >= 0 && m < BlendModeCount ? n[m] : "?";
}

// Inputs for custom (ShaderToy-style) shader effects.
struct UserFxParams {
    float time = 0, beat = 0, beatPhase = 0, bass = 0, mid = 0, treble = 0, level = 0, amount = 1, drop = 0, vocal = 0;
    int frame = 0;
};

class Gfx {
public:
    // Compile a user effect: the file defines mainImage(out vec4, in vec2) like ShaderToy. 0 + err on failure.
    unsigned compileUser(const std::string& src, std::string& err);
    void userFx(RenderTarget& out, unsigned prog, unsigned src, unsigned prev, const UserFxParams& p);
    bool init();
    void mix(RenderTarget& out, unsigned a, unsigned b, float x, int mode);
    // Transition from one deck to the other; progress 0..1 (0 = all "from").
    void transition(RenderTarget& out, unsigned from, unsigned to, float progress, int type, float time);
    void fx(RenderTarget& out, unsigned src, unsigned prev, const FxParams& p, float rotation, float time);
    // Draw a texture into the bound target inside an NDC rectangle.
    void layer(unsigned tex, float x0, float y0, float x1, float y1, float opacity, bool flipY, int blend);
    void finalize(RenderTarget& out, unsigned src, float black, float white);
    // Present into the bound default framebuffer viewport. corners = TL,TR,BR,BL (x,y) in 0..1, top-left origin.
    void present(unsigned tex, int x, int y, int w, int h, const float* corners, bool grid, bool flipY = false);

private:
    unsigned vao_ = 0, transP_ = 0, mixP_ = 0, fxP_ = 0, layerP_ = 0, finalP_ = 0, presentP_ = 0;
};
