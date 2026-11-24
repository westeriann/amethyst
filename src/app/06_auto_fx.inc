// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ---- Auto FX: while auto-pilot runs, every effect follows the song. Drops get a flash, a zoom punch,
// an RGB burst and an 8th-note strobe bar plus a strong new look; build-ups ramp zoom, swirl, RGB split,
// brightness and an accelerating strobe; fakeout gaps go dark and colourless; breakdowns get soft trails;
// grooves change look every 8-bar phrase; songs fade out and back in. Transition styles fit the moment.
struct AutoFx {
    int look = 0;
    double lookBeat = 0, dropBeat = -1e9, fakeBeat = -1e9, startT = -1e9, endT = -1e9;
    float rot = 0, white = 0, black = 0;
    int transition = -1;  // style for preset switches (-1 = the user's own setting)
    int shader = -1;      // custom shader Auto FX is using (-1 = none)
    long long lastPhrase = -(1LL << 40);
} afx;
bool autoFxOn() { return S.autoOn && S.autoFx; }
void autoFlash(float amt) { if (S.autoFxFlash) flashAmt = std::max(flashAmt, amt); }
void autoWhite(float amt) { if (S.autoFxFlash) afx.white = std::max(afx.white, amt); }

const int kDropLooks[] = {0, 1, 2, 3, 6, 7}, kCalmLooks[] = {2, 4, 5}, kGrooveLooks[] = {0, 1, 2, 4, 5, 6};
const char* kLookNames[] = {"Kaleidoscope spin", "Quad mirror + RGB", "Tunnel trails", "Beat pixels", "Colour cycle", "Swirl", "Kaleido trails", "Mirror flips"};
void pickLook(const int* set, int n) {
    int l = afx.look;
    for (int k = 0; k < 8 && (l == afx.look || k == 0); k++) l = set[rng() % n];
    afx.look = l;
    afx.lookBeat = beatNow();
}
int randomTransition() { return 1 + (int)(rng() % (TransitionCount - 1)); }
int smoothTransition() { const int t[] = {TransBlend, TransDissolve, TransLuma, TransBlend}; return t[rng() % 4]; }

void applyLook(int id, FxParams& f, float a, double beat, double dt) {
    float env = (float)std::exp(-(beat - std::floor(beat)) * 6);  // pulse on every beat
    switch (id) {
        case 0:
            f.kaleido = true;
            f.kaleidoSegments = 6 + 2 * ((int)afx.lookBeat % 3);
            afx.rot += (float)(dt * 0.5 * a);
            break;
        case 1:
            f.mirror = true, f.mirrorMode = 3;
            f.rgbSplitAmount = std::max(f.rgbSplit ? f.rgbSplitAmount : 0.f, 0.004f + 0.02f * a * env);
            f.rgbSplit = true;
            break;
        case 2:
            f.feedbackAmount = std::max(f.feedback ? f.feedbackAmount : 0.f, 0.7f + 0.2f * a);
            f.feedback = true, f.feedbackZoom = 1.02f + 0.02f * a, f.feedbackRotate = 0.01f * a;
            break;
        case 3:
            if (env > 0.25f) {
                f.pixelSize = std::max(f.pixelate ? f.pixelSize : 0.f, 3 + 28 * a * env);
                f.pixelate = true;
            }
            break;
        case 4:
            f.hue += (float)std::fmod(beat * 0.02, 1.0);
            f.hue -= std::floor(f.hue);
            f.saturation *= 1 + 0.4f * a, f.contrast *= 1 + 0.25f * a;
            break;
        case 5: f.swirl += (float)std::sin(beat * 3.14159 / 8) * 1.5f * a; break;
        case 6:
            f.kaleido = true, f.kaleidoSegments = 4;
            f.feedbackAmount = std::max(f.feedback ? f.feedbackAmount : 0.f, 0.6f + 0.25f * a);
            f.feedback = true, f.feedbackZoom = 0.98f;
            break;
        case 7:
            f.mirror = true, f.mirrorMode = 1;
            if (S.autoFxFlash && a > 0.4f && std::fmod(beat, 4.0) < 0.15) f.invert = !f.invert;
            break;
    }
}

void autoShaderPick(int chanceOutOf10) {
    afx.shader = S.autoFxShaders && !userShaders.empty() && (int)(rng() % 10) < chanceOutOf10 ? (int)(rng() % userShaders.size()) : -1;
}
void autoFxEvent(MusicStructure::Event e) {
    if (!autoFxOn()) return;
    float a = S.autoFxAmount;
    if (e == MusicStructure::DropHit) autoShaderPick(7);
    if (e == MusicStructure::BreakdownStart || e == MusicStructure::SongEnd) afx.shader = -1;
    switch (e) {
        case MusicStructure::DropHit:
            afx.dropBeat = beatNow();
            autoFlash(std::min(1.f, 0.4f + a));
            pickLook(kDropLooks, 6);
            afx.lastPhrase = (long long)std::floor(structure.nextBoundary(afx.dropBeat, 8));
            afx.transition = rng() % 2 ? TransZoom : TransGlitch;
            break;
        case MusicStructure::Fakeout: afx.fakeBeat = beatNow(); break;
        case MusicStructure::BreakdownStart:
            pickLook(kCalmLooks, 3);
            afx.transition = rng() % 2 ? TransDissolve : TransLuma;
            break;
        case MusicStructure::SongStart: afx.startT = now(), afx.endT = -1e9; break;
        case MusicStructure::SongChange:
            pickLook(kGrooveLooks, 6);
            afx.transition = smoothTransition();
            break;
        case MusicStructure::SongEnd: afx.endT = now(); break;
    }
}

void autoFxApply(double t, double dt) {
    afx.white = afx.black = 0;
    if (!autoFxOn()) return;
    FxParams& f = modOut.fx;
    float a = S.autoFxAmount;
    double b = beatNow(), fr = b - std::floor(b);
    float beatEnv = (float)std::exp(-fr * 6), barEnv = (float)std::exp(-(structure.beatInBar(b) + fr) * 3);
    auto sec = structure.section();
    long long phrase = (long long)std::floor(structure.nextBoundary(b, 8));
    bool newPhrase = phrase != afx.lastPhrase;
    if (S.vocalEase && sec != MusicStructure::Drop && sec != MusicStructure::Build) a *= 1 - 0.45f * structure.vocal();
    switch (sec) {
        case MusicStructure::Drop: {
            if (newPhrase) pickLook(kDropLooks, 6);
            applyLook(afx.look, f, a, b, dt);
            f.zoom *= 1 + 0.06f * a * beatEnv;
            double k = b - afx.dropBeat;
            if (k >= 0 && k < 4) {  // the first bar of the drop: zoom punch, RGB burst, 8th-note strobe
                f.zoom *= 1 + 0.3f * a * (float)std::exp(-k * 2);
                f.rgbSplitAmount = std::max(f.rgbSplit ? f.rgbSplitAmount : 0.f, 0.03f * a * (float)std::exp(-k * 1.5));
                f.rgbSplit = true;
                if (std::fmod(b * 2, 1.0) < 0.2) autoWhite(0.7f * a);
            }
            break;
        }
        case MusicStructure::Groove:
            if (newPhrase) {
                pickLook(kGrooveLooks, 6);
                autoShaderPick(3);
                if (!(S.vocalEase && structure.vocalsOn())) autoFlash(0.3f * a);
            }
            applyLook(afx.look, f, a * 0.6f, b, dt);
            f.zoom *= 1 + 0.04f * a * barEnv;
            break;
        case MusicStructure::Intro: applyLook(5, f, a * 0.3f, b, dt); break;
        case MusicStructure::Breakdown:
            applyLook(afx.look, f, a * 0.5f, b, dt);
            f.saturation *= 1 - 0.25f * a;
            f.feedbackAmount = std::max(f.feedback ? f.feedbackAmount : 0.f, 0.8f * a);
            f.feedback = true;
            break;
        case MusicStructure::Build: {
            if (afx.fakeBeat > afx.dropBeat && b - afx.fakeBeat < 16) {  // fakeout gap: hold your breath
                f.saturation = 0;
                f.brightness -= 0.3f * a;
                f.feedback = true, f.feedbackAmount = 0.95f;
                break;
            }
            float p = (float)std::clamp(structure.sectionBeats(b) / 32.0, 0.0, 1.0);  // full tension after 8 bars
            f.zoom *= 1 + 0.25f * a * p;
            f.swirl += 2.f * a * p * (float)std::sin(b * 3.14159 / 2);
            f.rgbSplitAmount = std::max(f.rgbSplit ? f.rgbSplitAmount : 0.f, 0.025f * a * p);
            f.rgbSplit = true;
            f.brightness += 0.12f * a * p;
            if (p > 0.3f) {  // strobe speeds up: quarters, eighths, sixteenths
                double rate = p < 0.6f ? 1 : p < 0.85f ? 2 : 4;
                if (std::fmod(b * rate, 1.0) < 0.18) autoWhite((0.25f + 0.5f * p) * a);
            }
            break;
        }
        case MusicStructure::Silence:
            if (t - afx.endT < 120) afx.black = (float)std::min(1.0, (t - afx.endT) / 2.0) * 0.85f * a;
            break;
    }
    afx.lastPhrase = phrase;
    if (t - afx.startT < 2) afx.black = std::max(afx.black, (float)(1 - (t - afx.startT) / 2) * 0.85f * a);  // fade in
    modOut.rotOffset += afx.rot;
}

