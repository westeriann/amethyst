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
    // groove texture: small moves on hi-hats and snares that keep quiet parts alive
    int tex = 0;
    long long hatSeen = 0, snSeen = 0;
    int swayDir = 1;
    float sway = 0, swayTarget = 0, hueStep = 0, hueSmooth = 0, texAmt = 0;
} afx;
bool autoFxOn() { return S.autoOn && S.autoFx; }
void autoFlash(float amt) { if (S.autoFxFlash) flashAmt = std::max(flashAmt, amt); }
void autoWhite(float amt) { if (S.autoFxFlash) afx.white = std::max(afx.white, amt); }

const char* kLookNames[] = {"Kaleidoscope spin", "Quad mirror + RGB", "Tunnel trails", "Beat pixels", "Colour cycle", "Swirl",
                            "Kaleido trails", "Mirror flips", "Mirror bloom", "Hue split", "Spin mirror", "Pixel melt", "Zoom pump",
                            "Dream wash"};
const int kLookCount = 14;
// How well each look suits a moment: calm (breakdowns, intros), groove, drop. 0 = never there.
const float kLookFit[kLookCount][3] = {
    {0.25f, 0.8f, 0.8f},  // kaleidoscope spin
    {0.15f, 0.8f, 1.0f},  // quad mirror + RGB
    {0.9f, 0.7f, 0.5f},   // tunnel trails
    {0.0f, 0.3f, 1.0f},   // beat pixels
    {0.8f, 0.8f, 0.4f},   // colour cycle
    {0.9f, 0.6f, 0.3f},   // swirl
    {0.45f, 0.5f, 0.7f},  // kaleido trails
    {0.0f, 0.35f, 1.0f},  // mirror flips
    {0.65f, 0.85f, 0.6f}, // mirror bloom
    {0.4f, 0.75f, 0.9f},  // hue split
    {0.55f, 0.7f, 0.6f},  // spin mirror
    {0.25f, 0.6f, 0.75f}, // pixel melt
    {0.15f, 0.7f, 0.95f}, // zoom pump
    {1.0f, 0.45f, 0.1f},  // dream wash
};
// Balanced choice: weight = how well it fits / how much it was used lately, never the one showing now.
// Usage decays with every pick, so an effect left alone for a while is fully fresh again.
int balancedPick(const float* fit, int n, std::vector<float>& use, int current) {
    if ((int)use.size() < n) use.resize(n, 0.f);
    float total = 0;
    std::vector<float> w(n);
    for (int i = 0; i < n; i++) {
        w[i] = i == current && n > 1 ? 0.f : fit[i] / (1.f + 2.f * use[i]);
        total += w[i];
    }
    int pick = current >= 0 && current < n ? current : 0;
    if (total > 0) {
        float r = std::uniform_real_distribution<float>(0.f, total)(rng);
        for (int i = 0; i < n; i++)
            if ((r -= w[i]) <= 0 && w[i] > 0) { pick = i; break; }
    }
    for (auto& u : use) u *= 0.8f;
    use[pick] += 1;
    return pick;
}
std::vector<float> lookUse, transUse, texUse, shaderUse;

// ---- Journey: a mood for the whole set. It steers which presets auto-pilot picks (colour, brightness,
// saturation, motion, energy) and which Auto FX looks it prefers, and tints the effects a little.
struct Journey {
    const char* name;
    const char* desc;
    float hues[3];  // preferred colours (0..1 round the wheel); nh = 0: any colour
    int nh;
    float bright, sat, motion;
    int energyBias;  // -1 calmer presets, +1 harder-hitting ones
    int looks[6];    // favourite Auto FX looks
    int nl;
    float satMul, brightAdd;
};
const Journey kJourneys[] = {
    {"Dark", "Deep, shadowy and moody. Low light, little colour.", {0}, 0, 0.12f, 0.35f, 0.22f, 0, {2, 13, 6, 8, 11}, 5, 0.8f, -0.05f},
    {"Happy", "Bright, warm and colourful: yellows, oranges, pinks, greens.", {0.11f, 0.9f, 0.3f}, 3, 0.58f, 0.75f, 0.3f, 0, {4, 9, 8, 12, 1}, 5, 1.2f, 0.03f},
    {"Dreamy", "Soft pastels drifting slowly: pinks, purples, sky blues.", {0.82f, 0.9f, 0.55f}, 3, 0.4f, 0.45f, 0.15f, -1, {5, 13, 2, 4, 10}, 5, 0.9f, 0.02f},
    {"Aggressive", "Hard, fast and red-hot. Big motion, harsh cuts.", {0.0f, 0.04f, 0.97f}, 3, 0.45f, 0.75f, 0.45f, 1, {3, 7, 1, 12, 11}, 5, 1.15f, 0.0f},
    {"Trippy", "Saturated, swirling, every colour at once.", {0}, 0, 0.5f, 0.9f, 0.4f, 0, {0, 9, 6, 4, 10, 5}, 6, 1.3f, 0.0f},
    {"Chill", "Calm blues, teals and greens with gentle movement.", {0.55f, 0.45f, 0.62f}, 3, 0.25f, 0.5f, 0.14f, -1, {5, 13, 4, 2}, 4, 0.95f, 0.0f},
    {"Cosmic", "Space: deep blues and violets, tunnels and stars.", {0.66f, 0.75f, 0.6f}, 3, 0.18f, 0.6f, 0.28f, 0, {2, 6, 10, 0, 13}, 5, 1.05f, -0.02f},
    {"Warm", "Sunset tones: amber, coral, magenta, golden light.", {0.05f, 0.09f, 0.93f}, 3, 0.48f, 0.65f, 0.25f, 0, {8, 13, 4, 2, 9}, 5, 1.1f, 0.02f},
};
const int kJourneyCount = 8;
int wanderMood = 0, wanderSongs = 0;
double wanderSince = 0;
// the mood in charge now, or nullptr when Journey is off
const Journey* journeyNow() {
    if (S.journey >= 1 && S.journey <= kJourneyCount) return &kJourneys[S.journey - 1];
    if (S.journey == kJourneyCount + 1) return &kJourneys[wanderMood % kJourneyCount];
    return nullptr;
}
const char* journeyLabel() { return S.journey == 0 ? "Off" : S.journey == kJourneyCount + 1 ? "Wander" : kJourneys[S.journey - 1].name; }
void wanderNext() {  // move to a related mood: neighbours in the list are close in feel
    int step = 1 + (int)(rng() % 3);
    wanderMood = (wanderMood + (rng() % 2 ? step : kJourneyCount - step)) % kJourneyCount;
    wanderSince = now(), wanderSongs = 0;
}

void pickLookFor(int mood) {
    float fit[kLookCount];
    for (int i = 0; i < kLookCount; i++) fit[i] = kLookFit[i][mood];
    if (const Journey* j = journeyNow())
        for (int i = 0; i < kLookCount; i++) {
            bool fav = std::find(j->looks, j->looks + j->nl, i) != j->looks + j->nl;
            fit[i] *= fav ? 2.2f : 0.5f;  // favourites lead, the rest still appear now and then
        }
    afx.look = balancedPick(fit, kLookCount, lookUse, afx.look);
    afx.lookBeat = beatNow();
}
const char* kTexNames[] = {"Hat shimmer", "Tick-tock sway", "Breathing", "Echo pulse", "Snare colour steps"};
void pickTexture() {
    bool perc = structure.hatsPerBeat() > 0.3f;  // no drums at all (pads, ambient): pick a look that moves by itself
    const float withDrums[] = {1.0f, 1.0f, 0.7f, 0.8f, 0.9f}, noDrums[] = {0.1f, 0.1f, 1.0f, 1.0f, 0.2f};
    afx.tex = balancedPick(perc ? withDrums : noDrums, 5, texUse, afx.tex);
}
// Groove texture: hats and snares drive gentle, small moves; a slow 4-bar breath underneath means
// even a drumless breakdown never sits still. Strongest where the bass is low.
void applyTexture(FxParams& f, float s, double b, double dt) {
    bool newHat = structure.hatHits() != afx.hatSeen, newSn = structure.snareHits() != afx.snSeen;
    afx.hatSeen = structure.hatHits(), afx.snSeen = structure.snareHits();
    afx.texAmt = s;
    if (s <= 0.01f) { afx.sway *= 0.9f; return; }
    float hat = structure.hat(), sn = structure.snare();
    const float PI = 3.14159265f;
    f.zoom *= 1 + 0.02f * s * (float)std::sin(b * PI / 8);
    switch (afx.tex) {
        case 0:  // shimmer: hats split the colours a hair and catch the light, snares add bite
            f.rgbSplitAmount = std::max(f.rgbSplit ? f.rgbSplitAmount : 0.f, 0.001f + 0.007f * s * hat);
            f.rgbSplit = true;
            f.brightness += 0.06f * s * hat;
            f.contrast *= 1 + 0.12f * s * sn;
            break;
        case 1:  // tick-tock: each hat tips the picture the other way, snares pop the zoom
            if (newHat) afx.swayDir = -afx.swayDir, afx.swayTarget = afx.swayDir * 0.035f * s;
            f.zoom *= 1 + 0.05f * s * sn;
            break;
        case 2:  // breathing: slow swirl and zoom over the bars, hats add a tiny tick
            f.swirl += 0.35f * s * (float)std::sin(b * PI / 4);
            f.zoom *= (1 + 0.03f * s * (float)std::sin(b * PI / 8)) * (1 + 0.015f * s * hat);
            f.hue += (float)std::fmod(b * 0.004 * s, 1.0);
            break;
        case 3:  // echo pulse: trails that swell on every hit
            f.feedbackAmount = std::min(0.93f, std::max(f.feedback ? f.feedbackAmount : 0.f, 0.5f + 0.35f * s * std::max(hat, sn)));
            f.feedback = true;
            f.feedbackZoom = std::max(f.feedbackZoom, 1.0f + 0.015f * s * (0.3f + sn));
            break;
        case 4:  // colour steps: each snare moves the hue one step round the wheel, hats lift saturation
            if (newSn) afx.hueStep += 1.f / 12;
            f.saturation *= 1 + 0.25f * s * hat;
            break;
    }
    afx.sway += (afx.swayTarget - afx.sway) * (float)(1 - std::exp(-dt / 0.06));
    afx.swayTarget *= (float)std::exp(-dt / 0.35);
    afx.hueSmooth += (afx.hueStep - afx.hueSmooth) * (float)(1 - std::exp(-dt / 0.12));
    if (afx.tex == 4) f.hue += afx.hueSmooth * std::min(1.f, s * 1.5f);
    f.hue -= std::floor(f.hue);
    modOut.rotOffset += afx.sway;
}

int randomTransition() { return 1 + (int)(rng() % (TransitionCount - 1)); }
// transition styles per moment: drops punch through, everything else melts
int pickTransition(bool drop) {
    static int last = -1;
    //                       Blend  Wipe  Iris  Zoom  Glitch Dissolve Luma  Slide
    const float dropFit[] = {0.0f, 0.3f, 0.6f, 1.0f, 1.0f, 0.0f, 0.0f, 0.6f};
    const float calmFit[] = {1.0f, 0.25f, 0.35f, 0.0f, 0.0f, 0.9f, 0.9f, 0.2f};
    last = balancedPick(drop ? dropFit : calmFit, TransitionCount, transUse, last);
    return last;
}
int smoothTransition() { return pickTransition(false); }

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
        case 8:  // mirror bloom: vertical mirror, richer colour and contrast that breathe with the beat
            f.mirror = true, f.mirrorMode = 2;
            f.saturation *= 1 + 0.35f * a, f.contrast *= 1 + 0.15f * a * env, f.brightness += 0.04f * a * env;
            break;
        case 9:  // hue split: colours drift round the wheel while a small RGB split pulses
            f.hue += (float)std::fmod(beat * 0.01, 1.0), f.hue -= std::floor(f.hue);
            f.rgbSplitAmount = std::max(f.rgbSplit ? f.rgbSplitAmount : 0.f, 0.003f + 0.012f * a * env);
            f.rgbSplit = true;
            break;
        case 10:  // spin mirror: quad mirror that slowly turns, direction flips every 8 bars
            f.mirror = true, f.mirrorMode = 3;
            afx.rot += (float)(dt * 0.25 * a * (((long long)std::floor(beat / 32)) % 2 ? 1 : -1));
            break;
        case 11:  // pixel melt: soft pixels smeared by trails, sharper on the beat
            f.pixelSize = std::max(f.pixelate ? f.pixelSize : 0.f, 2 + 5 * a * (1 - env));
            f.pixelate = true;
            f.feedbackAmount = std::max(f.feedback ? f.feedbackAmount : 0.f, 0.55f + 0.2f * a);
            f.feedback = true, f.feedbackZoom = 1.0f, f.feedbackRotate = 0.004f * a;
            break;
        case 12:  // zoom pump: the picture punches in on every beat and turns a hair against it
            f.zoom *= 1 + 0.09f * a * env;
            afx.rot -= (float)(dt * 0.12 * a * env);
            f.contrast *= 1 + 0.1f * a * env;
            break;
        case 13:  // dream wash: long soft trails, washed colours, a slow colour drift
            f.feedbackAmount = std::max(f.feedback ? f.feedbackAmount : 0.f, 0.86f);
            f.feedback = true, f.feedbackZoom = 1.006f, f.feedbackRotate = 0.002f;
            f.saturation *= 1 - 0.3f * a;
            f.hue += (float)std::fmod(beat * 0.003, 1.0), f.hue -= std::floor(f.hue);
            break;
    }
}

void autoShaderPick(int chanceOutOf10, int mood = 1) {
    if (!S.autoFxShaders || userShaders.empty() || (int)(rng() % 10) >= chanceOutOf10) { afx.shader = -1; return; }
    std::vector<float> fit(userShaders.size());
    for (size_t i = 0; i < fit.size(); i++) fit[i] = userShaders[i].fit[std::clamp(mood, 0, 2)];
    afx.shader = balancedPick(fit.data(), (int)fit.size(), shaderUse, afx.shader);
}
void autoFxEvent(MusicStructure::Event e) {
    if (!autoFxOn()) return;
    float a = S.autoFxAmount;
    if (e == MusicStructure::DropHit) autoShaderPick(7, 2);
    if (e == MusicStructure::BreakdownStart) autoShaderPick(3, 0);  // sometimes a soft one (liquid, sparkle, bloom) in breakdowns
    if (e == MusicStructure::SongEnd) afx.shader = -1;
    if ((e == MusicStructure::SongChange || e == MusicStructure::SongStart) && S.journey == kJourneyCount + 1 && ++wanderSongs >= 2) wanderNext();
    switch (e) {
        case MusicStructure::DropHit:
            afx.dropBeat = beatNow();
            autoFlash(std::min(1.f, 0.4f + a));
            pickLookFor(2);
            afx.lastPhrase = (long long)std::floor(structure.nextBoundary(afx.dropBeat, 8));
            afx.transition = pickTransition(true);
            break;
        case MusicStructure::Fakeout: afx.fakeBeat = beatNow(); break;
        case MusicStructure::BreakdownStart:
            pickLookFor(0);
            pickTexture();
            afx.transition = smoothTransition();
            break;
        case MusicStructure::SongStart: afx.startT = now(), afx.endT = -1e9, pickTexture(); break;
        case MusicStructure::SongChange:
            pickLookFor(1);
            pickTexture();
            afx.transition = smoothTransition();
            break;
        case MusicStructure::SongEnd: afx.endT = now(); break;
    }
}

void autoFxApply(double t, double dt) {
    afx.white = afx.black = 0;
    if (!autoFxOn()) { afx.texAmt = 0; return; }
    FxParams& f = modOut.fx;
    float a = S.autoFxAmount;
    double b = beatNow(), fr = b - std::floor(b);
    float beatEnv = (float)std::exp(-fr * 6), barEnv = (float)std::exp(-(structure.beatInBar(b) + fr) * 3);
    auto sec = structure.section();
    long long phrase = (long long)std::floor(structure.nextBoundary(b, 8));
    bool newPhrase = phrase != afx.lastPhrase;
    if (S.vocalEase && sec != MusicStructure::Drop && sec != MusicStructure::Build) a *= 1 - 0.45f * structure.vocal();
    if (newPhrase && sec != MusicStructure::Drop && sec != MusicStructure::Build) pickTexture();
    if (S.journey == kJourneyCount + 1 && t - wanderSince > 360 && newPhrase) wanderNext();  // long mixes wander on too
    if (const Journey* j = journeyNow()) f.saturation *= 1 + (j->satMul - 1) * std::min(1.f, a / 0.7f), f.brightness += j->brightAdd * std::min(1.f, a / 0.7f);
    {  // groove texture strength: full in breakdowns and intros, in grooves more the lower the bass is
        float calm = std::clamp(1.15f - structure.energy(), 0.f, 1.f), k = 0;
        bool gap = afx.fakeBeat > afx.dropBeat && b - afx.fakeBeat < 16;
        switch (sec) {
            case MusicStructure::Breakdown: k = 1; break;
            case MusicStructure::Intro: k = 0.8f; break;
            case MusicStructure::Groove: k = 0.5f + 0.5f * calm; break;
            case MusicStructure::Build: k = gap ? 0 : 0.5f; break;
            case MusicStructure::Drop: k = 0.3f; break;
            default: break;
        }
        applyTexture(f, S.autoTexture * std::min(1.f, a / 0.7f) * k, b, dt);
    }
    switch (sec) {
        case MusicStructure::Drop: {
            if (newPhrase) pickLookFor(2);
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
                pickLookFor(structure.energy() < 0.6f ? 0 : 1);  // a quiet groove gets the calmer looks
                autoShaderPick(3, structure.energy() < 0.6f ? 0 : 1);
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

