// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== advanced features
void saveScenes();

// ---- modulation routing
enum ModSrc { SrcBass, SrcMid, SrcTreble, SrcLevel, SrcBeat, SrcLfoSine, SrcLfoSaw, SrcLfoSquare, SrcLfoRandom, SrcVocal, SrcEnergy, SrcHats, SrcSnare, SrcCount };
const char* kModSrcNames[] = {"Bass", "Mids", "Treble", "Level", "Beat pulse", "LFO sine", "LFO saw", "LFO square", "LFO random", "Vocals", "Song energy", "Hi-hat hits", "Snare / clap hits"};
bool isLfo(int src) { return src >= SrcLfoSine && src <= SrcLfoRandom; }
enum ModDst { DstZoom, DstRotate, DstSwirl, DstHue, DstSat, DstContrast, DstBright, DstRgb, DstFeedback, DstPixel, DstKaleido,
              DstCrossfade, DstVideoOpacity, DstLogoOpacity, DstTextOpacity, DstCount };
const char* kModDstNames[] = {"Zoom", "Rotate", "Swirl", "Hue", "Saturation", "Contrast", "Brightness", "RGB split", "Feedback trails",
                              "Pixelate", "Kaleidoscope segments", "Crossfader", "Video opacity", "Logo opacity", "Text opacity"};
struct ModRoute {
    int src = SrcBass, dst = DstZoom;
    float amount = 0.3f, rate = 4, smooth = 0.5f;  // rate = LFO period in beats
    bool on = true;
    float state = 0;
};
std::vector<ModRoute> mods;
float modSource(int src, double beat) {
    auto frac = [](double x) { return (float)(x - std::floor(x)); };
    switch (src) {
        case SrcBass: return bandBass;
        case SrcMid: return bandMid;
        case SrcTreble: return bandTreb;
        case SrcLevel: return bandLevel;
        case SrcBeat: { float f = 1 - frac(beat); return f * f; }
        case SrcVocal: return structure.vocal();
        case SrcEnergy: return std::min(1.f, structure.energy());
        case SrcHats: return structure.hat();
        case SrcSnare: return structure.snare();
        default: break;
    }
    return 0;
}
float lfo(int src, double beat, float rate) {
    double ph = beat / std::max(0.0625f, rate);
    float f = (float)(ph - std::floor(ph));
    switch (src) {
        case SrcLfoSine: return 0.5f + 0.5f * std::sin(f * 6.2831853f);
        case SrcLfoSaw: return f;
        case SrcLfoSquare: return f < 0.5f ? 1.f : 0.f;
        case SrcLfoRandom: { double k = std::floor(ph); return (float)(std::fmod(std::sin(k * 12.9898) * 43758.5453, 1.0) + 1) * 0.5f; }
        default: return 0;
    }
}
struct ModOut { FxParams fx; float xfade, rotOffset, videoOpacity, logoOpacity, textOpacity; } modOut;
void applyModulation(double beat) {
    ModOut m{S.fx, S.xfade, 0, S.videoOpacity, S.logoOpacity, S.textOpacity};
    for (auto& r : mods) {
        if (!r.on) { r.state = 0; continue; }
        float v = isLfo(r.src) ? lfo(r.src, beat, r.rate) : modSource(r.src, beat);
        r.state += (v - r.state) * (1 - std::clamp(r.smooth, 0.f, 0.97f));
        float a = r.amount * r.state;
        FxParams& f = m.fx;
        switch (r.dst) {
            case DstZoom: f.zoom *= std::max(0.1f, 1 + a); break;
            case DstRotate: m.rotOffset += a * 3.14159f; break;
            case DstSwirl: f.swirl += a * 3; break;
            case DstHue: f.hue = f.hue + a - std::floor(f.hue + a); break;
            case DstSat: f.saturation = std::max(0.f, f.saturation * (1 + a * 1.5f)); break;
            case DstContrast: f.contrast = std::max(0.1f, f.contrast * (1 + a)); break;
            case DstBright: f.brightness += a * 0.5f; break;
            case DstRgb: if (a > 0.01f) { f.rgbSplitAmount = (f.rgbSplit ? f.rgbSplitAmount : 0) + a * 0.03f; f.rgbSplit = true; } break;
            case DstFeedback: if (f.feedback || a > 0.02f) { f.feedbackAmount = std::clamp((f.feedback ? f.feedbackAmount : 0) + a, 0.f, 0.97f); f.feedback = true; } break;
            case DstPixel: if (a > 0.05f) { f.pixelSize = (f.pixelate ? f.pixelSize : 2) + a * 40; f.pixelate = true; } break;
            case DstKaleido: if (a > 0.08f) { f.kaleidoSegments = std::clamp((f.kaleido ? f.kaleidoSegments : 2) + (int)std::round(a * 10), 2, 24); f.kaleido = true; } break;
            case DstCrossfade: m.xfade = std::clamp(m.xfade + a, 0.f, 1.f); break;
            case DstVideoOpacity: m.videoOpacity = std::clamp(m.videoOpacity + a, 0.f, 1.f); break;
            case DstLogoOpacity: m.logoOpacity = std::clamp(m.logoOpacity + a, 0.f, 1.f); break;
            case DstTextOpacity: m.textOpacity = std::clamp(m.textOpacity + a, 0.f, 1.f); break;
        }
    }
    modOut = m;
}

// ---- scenes (whole looks) with morphing, and a beat-based show timeline
struct Scene {
    bool used = false;
    std::string name, presetA, presetB, text;
    float xfade = 0, videoOpacity = 0.6f;
    int mixMode = 0;
    bool textOn = false, logoOn = false, videoOn = false;
    FxParams fx;
};
constexpr int kScenes = 16;
Scene scenes[kScenes];
int activeScene = -1;
struct Morph { bool active = false; FxParams from, to; float xFrom = 0, xTo = 0; double startBeat = 0, beats = 0; } morph;
struct TlStep { int scene = 0; int bars = 4; };
std::vector<TlStep> timeline;
bool tlPlaying = false, tlLoop = true;
int tlIndex = 0;
double tlStepStart = 0;

std::vector<Field> fxFields(FxParams& f) {
    return {{"kaleido", 'b', &f.kaleido}, {"kaleido_n", 'i', &f.kaleidoSegments}, {"mirror", 'b', &f.mirror}, {"mirror_mode", 'i', &f.mirrorMode},
            {"feedback", 'b', &f.feedback}, {"feedback_amt", 'f', &f.feedbackAmount}, {"feedback_zoom", 'f', &f.feedbackZoom},
            {"feedback_rot", 'f', &f.feedbackRotate}, {"rgb", 'b', &f.rgbSplit}, {"rgb_amt", 'f', &f.rgbSplitAmount}, {"pixel", 'b', &f.pixelate},
            {"pixel_size", 'f', &f.pixelSize}, {"invert", 'b', &f.invert}, {"strobe", 'b', &f.strobe}, {"strobe_rate", 'f', &f.strobeRate},
            {"strobe_beat", 'b', &f.strobeOnBeat}, {"zoom", 'f', &f.zoom}, {"rotate", 'f', &f.rotateSpeed}, {"swirl", 'f', &f.swirl},
            {"hue", 'f', &f.hue}, {"sat", 'f', &f.saturation}, {"contrast", 'f', &f.contrast}, {"bright", 'f', &f.brightness}};
}

FxParams lerpFx(const FxParams& a, const FxParams& b, float k) {
    FxParams r = b;  // switches take the destination value immediately
    auto L = [k](float x, float y) { return x + (y - x) * k; };
    r.kaleidoSegments = (int)std::round(L((float)a.kaleidoSegments, (float)b.kaleidoSegments));
    r.feedbackAmount = L(a.feedbackAmount, b.feedbackAmount);
    r.feedbackZoom = L(a.feedbackZoom, b.feedbackZoom);
    r.feedbackRotate = L(a.feedbackRotate, b.feedbackRotate);
    r.rgbSplitAmount = L(a.rgbSplitAmount, b.rgbSplitAmount);
    r.pixelSize = L(a.pixelSize, b.pixelSize);
    r.strobeRate = L(a.strobeRate, b.strobeRate);
    r.zoom = L(a.zoom, b.zoom);
    r.rotateSpeed = L(a.rotateSpeed, b.rotateSpeed);
    r.swirl = L(a.swirl, b.swirl);
    float dh = b.hue - a.hue;  // shortest way round the colour wheel
    if (dh > 0.5f) dh -= 1;
    if (dh < -0.5f) dh += 1;
    r.hue = a.hue + dh * k - std::floor(a.hue + dh * k);
    r.saturation = L(a.saturation, b.saturation);
    r.contrast = L(a.contrast, b.contrast);
    r.brightness = L(a.brightness, b.brightness);
    return r;
}

void storeScene(int i) {
    if (i < 0 || i >= kScenes) return;
    Scene& s = scenes[i];
    s.used = true;
    if (s.name.empty()) s.name = "Scene " + std::to_string(i + 1);
    s.presetA = decks[0].cur >= 0 ? lib[decks[0].cur].path : "";
    s.presetB = decks[1].cur >= 0 ? lib[decks[1].cur].path : "";
    s.xfade = S.xfade;
    s.mixMode = S.mixMode;
    s.fx = S.fx;
    s.textOn = S.textOn;
    s.text = S.text;
    s.logoOn = S.logoOn;
    s.videoOn = S.videoOn;
    s.videoOpacity = S.videoOpacity;
    activeScene = i;
    saveScenes();
    toast(false, "Stored %s", s.name.c_str());
}

void recallScene(int i) {
    if (i < 0 || i >= kScenes || !scenes[i].used) { toast(false, "Scene %d is empty — store one with Ctrl+Shift+F%d or in the Scenes panel", i + 1, i + 1); return; }
    Scene& s = scenes[i];
    for (int d = 0; d < 2; d++) {
        auto it = byPath.find(d ? s.presetB : s.presetA);
        if (it != byPath.end() && it->second != decks[d].cur) loadPreset(d, it->second, false);
    }
    S.mixMode = s.mixMode;
    S.textOn = s.textOn;
    S.text = s.text;
    S.logoOn = s.logoOn;
    S.videoOn = s.videoOn;
    S.videoOpacity = s.videoOpacity;
    if (S.sceneMorphBeats <= 0.01f) {
        S.fx = s.fx;
        S.xfade = s.xfade;
        morph.active = false;
    } else {
        morph = {true, S.fx, s.fx, S.xfade, s.xfade, beatNow(), S.sceneMorphBeats};
    }
    xanim.active = false;
    activeScene = i;
    toast(false, "%s%s", s.name.c_str(), S.sceneMorphBeats > 0.01f ? "  (morphing)" : "");
}

void updateMorph() {
    if (!morph.active) return;
    float k = (float)std::clamp((beatNow() - morph.startBeat) / std::max(0.01, morph.beats), 0.0, 1.0);
    float e = k * k * (3 - 2 * k);
    S.fx = lerpFx(morph.from, morph.to, e);
    S.xfade = morph.xFrom + (morph.xTo - morph.xFrom) * e;
    if (k >= 1) { S.fx = morph.to; morph.active = false; }
}

void timelinePlay(bool on) {
    tlPlaying = on && !timeline.empty();
    if (tlPlaying) {
        tlIndex = 0;
        tlStepStart = std::floor(beatNow() / 4) * 4;
        recallScene(timeline[0].scene);
        toast(false, "Show timeline playing (%d steps)", (int)timeline.size());
    } else toast(false, "Show timeline stopped");
}

void updateTimeline() {
    if (!tlPlaying || timeline.empty()) return;
    double beat = beatNow();
    double len = std::max(1, timeline[tlIndex].bars) * 4.0;
    if (beat < tlStepStart + len) return;
    tlStepStart += len;
    if (beat > tlStepStart + 16) tlStepStart = std::floor(beat / 4) * 4;  // clock jumped: resync to the bar
    tlIndex++;
    if (tlIndex >= (int)timeline.size()) {
        if (!tlLoop) { tlPlaying = false; tlIndex = 0; toast(false, "Show timeline finished"); return; }
        tlIndex = 0;
    }
    recallScene(timeline[tlIndex].scene);
}

void saveScenes() {
    std::ofstream f(fs::u8path(gScenesPath), std::ios::trunc);
    if (!f) return;
    for (int i = 0; i < kScenes; i++) {
        Scene& s = scenes[i];
        if (!s.used) continue;
        f << "scene=" << i << "|" << s.name << "|" << s.presetA << "|" << s.presetB << "|" << s.xfade << "|" << s.mixMode << "|" << s.textOn << "|"
          << s.logoOn << "|" << s.videoOn << "|" << s.videoOpacity << "|" << s.text << "\n";
        f << "scenefx=" << i;
        for (auto& fl : fxFields(s.fx)) {
            f << "|" << fl.key << ":";
            if (fl.type == 'f') f << *(float*)fl.p; else if (fl.type == 'i') f << *(int*)fl.p; else f << (*(bool*)fl.p ? 1 : 0);
        }
        f << "\n";
    }
    for (auto& st : timeline) f << "step=" << st.scene << "|" << st.bars << "\n";
    f << "timeline_loop=" << (tlLoop ? 1 : 0) << "\n";
    for (auto& m : mods) f << "mod=" << m.src << "|" << m.dst << "|" << m.amount << "|" << m.rate << "|" << m.smooth << "|" << (m.on ? 1 : 0) << "\n";
}

void loadScenes() {
    std::ifstream f(fs::u8path(gScenesPath));
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto p = splitBar(v);
        if (k == "scene" && p.size() >= 11) {
            int i = std::atoi(p[0].c_str());
            if (i < 0 || i >= kScenes) continue;
            Scene& s = scenes[i];
            s.used = true;
            s.name = p[1]; s.presetA = p[2]; s.presetB = p[3];
            s.xfade = (float)std::atof(p[4].c_str()); s.mixMode = std::atoi(p[5].c_str());
            s.textOn = p[6] == "1"; s.logoOn = p[7] == "1"; s.videoOn = p[8] == "1";
            s.videoOpacity = (float)std::atof(p[9].c_str());
            s.text = p[10];
            for (size_t x = 11; x < p.size(); x++) s.text += "|" + p[x];
        } else if (k == "scenefx" && !p.empty()) {
            int i = std::atoi(p[0].c_str());
            if (i < 0 || i >= kScenes) continue;
            auto ff = fxFields(scenes[i].fx);
            for (size_t x = 1; x < p.size(); x++) {
                auto c = p[x].find(':');
                if (c == std::string::npos) continue;
                std::string key = p[x].substr(0, c), val = p[x].substr(c + 1);
                for (auto& fl : ff)
                    if (key == fl.key) {
                        if (fl.type == 'f') *(float*)fl.p = (float)std::atof(val.c_str());
                        else if (fl.type == 'i') *(int*)fl.p = std::atoi(val.c_str());
                        else *(bool*)fl.p = val == "1";
                    }
            }
        } else if (k == "step" && p.size() == 2) timeline.push_back({std::clamp(std::atoi(p[0].c_str()), 0, kScenes - 1), std::max(1, std::atoi(p[1].c_str()))});
        else if (k == "timeline_loop") tlLoop = v == "1";
        else if (k == "mod" && p.size() == 6) {
            ModRoute m;
            m.src = std::clamp(std::atoi(p[0].c_str()), 0, SrcCount - 1);
            m.dst = std::clamp(std::atoi(p[1].c_str()), 0, DstCount - 1);
            m.amount = (float)std::atof(p[2].c_str());
            m.rate = (float)std::atof(p[3].c_str());
            m.smooth = (float)std::atof(p[4].c_str());
            m.on = p[5] == "1";
            mods.push_back(m);
        }
    }
}

// ---- deck transitions (used whenever the crossfader is animated)
double transitionSeconds() {
    if (S.transBeats > 0.01f) return S.transBeats * 60.0 / std::max(30.0, tempo.bpm());
    return std::max(0.5f, S.blend);
}

// ---- live input layer: Spout receive
SpoutReceiver spoutRx;
GLuint spoutRxTex = 0;
int spoutRxW = 0, spoutRxH = 0;
std::string spoutRxConnected;
bool receiveSpout() {
    if (S.spoutInput.empty()) spoutRx.SetReceiverName(nullptr);
    else spoutRx.SetReceiverName(S.spoutInput.c_str());
    if (!spoutRxTex) {
        glGenTextures(1, &spoutRxTex);
        glBindTexture(GL_TEXTURE_2D, spoutRxTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if (!spoutRx.ReceiveTexture(spoutRxTex, GL_TEXTURE_2D, false, 0)) return false;
    if (spoutRx.IsUpdated()) {
        spoutRxW = (int)spoutRx.GetSenderWidth();
        spoutRxH = (int)spoutRx.GetSenderHeight();
        glBindTexture(GL_TEXTURE_2D, spoutRxTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, spoutRxW, spoutRxH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        spoutRxConnected = spoutRx.GetSenderName();
        return false;  // texture re-sized; next frame receives into it
    }
    return spoutRxW > 0;
}
std::vector<std::string> spoutSenders() {
    std::vector<std::string> out;
    int n = spoutRx.GetSenderCount();
    for (int i = 0; i < n; i++) {
        char name[256];
        if (spoutRx.GetSender(i, name, sizeof name)) out.push_back(name);
    }
    return out;
}
void openLayerSource() {
    std::string err;
    if (S.layerSource == 1) {
        if (!video.openCamera(S.cameraIndex, err)) toast(true, "Camera: %s", err.c_str());
        else toast(false, "Camera layer: %s", video.name().c_str());
    } else if (S.layerSource == 0 && !S.videoPath.empty()) {
        if (!video.open(S.videoPath, err)) toast(true, "Video: %s", err.c_str());
    } else {
        video.close();
    }
    videoSerial = -1;
}

// ---- Art-Net / DMX lighting
ArtNet artnet;
RenderTarget dmxRT;
double lastDmx = 0;
std::vector<std::array<unsigned char, 3>> dmxColors;
unsigned char dmxData[512];
const char* kDmxModes[] = {"RGB (3 ch)", "RGBW (4 ch)", "Dimmer + RGB (4 ch)", "Dimmer + RGB + Strobe (5 ch)"};
const int kDmxWidth[] = {3, 4, 4, 5};
void sendDmx(double t, float strobeWhite) {
    if (!S.dmxOn || t - lastDmx < 1.0 / 40) return;
    lastDmx = t;
    int n = std::clamp(S.dmxFixtures, 1, 64);
    dmxRT.ensure(n, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, dmxRT.fbo);
    gfx.present(finalRT.tex, 0, 0, n, 1, nullptr, false);
    std::vector<unsigned char> px((size_t)n * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, n, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    dmxColors.resize(n);
    memset(dmxData, 0, sizeof dmxData);
    int w = kDmxWidth[std::clamp(S.dmxMode, 0, 3)];
    float lvl = std::clamp(S.dmxLevel, 0.f, 1.f);
    for (int i = 0; i < n; i++) {
        unsigned char r = (unsigned char)(px[i * 4] * lvl), g = (unsigned char)(px[i * 4 + 1] * lvl), b = (unsigned char)(px[i * 4 + 2] * lvl);
        dmxColors[i] = {r, g, b};
        int base = S.dmxStart - 1 + i * w;
        if (base < 0 || base + w > 512) break;
        unsigned char* c = dmxData + base;
        switch (S.dmxMode) {
            case 0: c[0] = r; c[1] = g; c[2] = b; break;
            case 1: { unsigned char wv = std::min(r, std::min(g, b)); c[0] = r - wv; c[1] = g - wv; c[2] = b - wv; c[3] = wv; break; }
            case 2: c[0] = 255; c[1] = r; c[2] = g; c[3] = b; break;
            default: c[0] = 255; c[1] = r; c[2] = g; c[3] = b; c[4] = strobeWhite > 0.5f ? 255 : 0; break;
        }
    }
    artnet.send(S.dmxIp, S.dmxUniverse, dmxData, std::min(512, S.dmxStart - 1 + n * w));
}

// ---- DJ link (Denon StageLinQ)
StageLinq djLink;
StageLinq::Snapshot djSnap;
std::string djLastTitle;
double djTitleUntil = 0;
std::string djTitleText;
void setDjLink(bool on) {
    S.djOn = on;
    if (on) djLink.start(); else { djLink.stop(); if (tempo.source() == Tempo::Source::External) tempo.setSource(Tempo::Source::Auto); }
    toast(false, on ? "DJ link on — searching for Engine OS devices" : "DJ link off");
}
void updateDjLink(double t) {
    if (rj.active) return;  // the file being rendered is the only music
    if (!S.djOn) return;
    djSnap = djLink.snapshot();
    int on = StageLinq::onAirDeck(djSnap);
    if (on < 0) return;
    const DjDeck& d = djSnap.decks[on];
    if (S.djTempo && d.bpm > 0) {
        double beat = d.hasBeat ? d.beat + (t - d.beatTime) * d.bpm / 60.0 : tempo.beatAt(t);
        tempo.setExternal(d.bpm, beat, t);
    }
    if (!d.title.empty() && d.title != djLastTitle) {
        bool first = djLastTitle.empty();
        djLastTitle = d.title;
        djTitleText = d.artist.empty() ? d.title : d.artist + "  -  " + d.title;
        if (!first) structure.trackChanged(t, tempo.beatAt(t));
        if (S.djShowTitle) djTitleUntil = t + S.djTitleSecs;
        if (!first) {
            switch (S.djTrackAction) {
                case 1: go(focus, 1, false); break;
                case 2: go(focus, 1, false, true); break;
                case 3: {
                    int next = activeScene;
                    for (int k = 1; k <= kScenes; k++) {
                        int c = (activeScene + k + kScenes) % kScenes;
                        if (scenes[c].used) { next = c; break; }
                    }
                    if (next >= 0) recallScene(next);
                    break;
                }
                default: break;
            }
        }
        toast(false, "Now playing: %s", djTitleText.c_str());
    }
}

// ---- MIDI LED feedback + controller templates
std::map<std::string, int> ledCache;
double lastLed = 0;
int paletteFor(const std::string& group, bool bright) {
    static const std::map<std::string, std::pair<int, int>> pal = {
        {"Presets", {53, 55}}, {"Cues", {9, 11}}, {"Mixer", {37, 39}}, {"FX", {21, 23}}, {"Performance", {5, 7}}, {"Auto-pilot", {45, 47}},
        {"Tempo", {57, 59}}, {"Output", {3, 1}}, {"Scenes", {13, 15}}, {"Overlays", {3, 1}}, {"Library", {13, 15}}, {"View", {1, 1}}};
    auto it = pal.find(group);
    auto p = it == pal.end() ? std::make_pair(3, 1) : it->second;
    return bright ? p.first : p.second;
}
void updateLeds(double t, bool force = false) {
    if (!force && t - lastLed < 0.05) return;
    lastLed = t;
    for (auto& b : midi.bindings) {
        Action* a = actions.find(b.action);
        if (!a || a->kind == Action::Continuous) continue;
        int style = midi.feedbackStyle.count(b.device) ? midi.feedbackStyle[b.device] : 0;
        int level;  // 0 off, 1 dim (available), 2 bright (on/active)
        if (a->id.rfind("cue", 0) == 0 && a->id.size() == 4) {
            int slot = (a->id[3] - '0' + 9) % 10;
            auto it = S.cues[slot].empty() ? byPath.end() : byPath.find(S.cues[slot]);
            level = it == byPath.end() ? 0 : (it->second == decks[0].cur || it->second == decks[1].cur) ? 2 : 1;
        } else if (a->id.rfind("scene", 0) == 0 && a->id.size() > 5 && isdigit((unsigned char)a->id[5])) {
            int i = std::atoi(a->id.c_str() + 5) - 1;
            level = i < 0 || i >= kScenes || !scenes[i].used ? 0 : i == activeScene ? 2 : 1;
        } else if (a->get) level = a->get() > 0.5f ? 2 : 1;
        else level = 1;
        int vel = style >= 1 ? (level == 0 ? 0 : paletteFor(a->group, level == 2)) : (level == 2 ? 127 : 0);
        std::string key = b.device + "|" + std::to_string(b.type) + "|" + std::to_string(b.channel) + "|" + std::to_string(b.number);
        auto c = ledCache.find(key);
        if (!force && c != ledCache.end() && c->second == vel) continue;
        ledCache[key] = vel;
        midi.send(b.device, (unsigned char)((b.type ? 0xB0 : 0x90) | b.channel), (unsigned char)b.number, (unsigned char)vel);
    }
}

// Launchpad programmer mode SysEx per model
std::vector<unsigned char> launchpadSysex(int model) {
    unsigned char id = model == 1 ? 0x0D : model == 2 ? 0x0C : 0x0E;  // Mini MK3, X, Pro MK3
    return {0xF0, 0x00, 0x20, 0x29, 0x02, id, 0x0E, 0x01, 0xF7};
}
const char* kTemplates[] = {"Novation Launchpad Mini MK3", "Novation Launchpad X", "Novation Launchpad Pro MK3", "Akai APC40 mkII"};
void applyTemplate(int tpl, const std::string& device) {
    midi.bindings.erase(std::remove_if(midi.bindings.begin(), midi.bindings.end(), [&](const MidiBinding& b) { return b.device == device; }), midi.bindings.end());
    auto note = [&](int n, const char* id, int ch = 0) { midi.bindings.push_back({id, device, 0, ch, n}); };
    auto cc = [&](int n, const char* id, int ch = 0) { midi.bindings.push_back({id, device, 1, ch, n}); };
    const char* rows[5][8] = {
        {"cue1", "cue2", "cue3", "cue4", "cue5", "cue6", "cue7", "cue8"},
        {"cue9", "cue0", "scene1", "scene2", "scene3", "scene4", "scene5", "scene6"},
        {"fx_kaleido", "fx_mirror", "fx_feedback", "fx_rgb", "fx_pixel", "fx_invert", "fx_strobe", "blackout"},
        {"prev", "next", "random", "auto", "shuffle", "quantize", "tap", "flash"},
        {"a_next", "a_random", "b_next", "b_random", "focus_toggle", "xfade_auto", "mix_mode", "lock"}};
    if (tpl <= 2) {  // Launchpad programmer mode: note = row*10 + col, row 8 at the top
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 8; c++) note((8 - r) * 10 + c + 1, rows[r][c]);
        const char* row3[8] = {"scene7", "scene8", "timeline_play", "record", "spout", "logo", "text", "video"};
        for (int c = 0; c < 8; c++) note(30 + c + 1, row3[c]);
        cc(89, "xfade_left"); cc(79, "xfade_right"); cc(69, "speed_down"); cc(59, "speed_up"); cc(49, "transition");
        midi.feedbackStyle[device] = 2 + tpl;
        midi.sendSysex(device, launchpadSysex(tpl + 1));
    } else {  // APC40 mkII: clip grid notes 0..39 (row 0 at the bottom), ch 1
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 8; c++) note((4 - r) * 8 + c, rows[r][c]);
        const char* sceneBtns[5] = {"scene7", "scene8", "timeline_play", "record", "spout"};
        for (int i = 0; i < 5; i++) note(82 + i, sceneBtns[i]);
        note(99, "tap");
        cc(15, "crossfade");
        cc(14, "gain");
        const char* knobs[8] = {"fx_hue", "fx_zoom", "fx_rotate", "fx_swirl", "fx_feedback_amt", "speed", "video_opacity", "blend"};
        for (int i = 0; i < 8; i++) cc(16 + i, knobs[i]);
        midi.feedbackStyle[device] = 1;
    }
    ledCache.clear();
    saveSettings();
    toast(false, "Applied %s template to %s (%d controls)", kTemplates[tpl], device.c_str(), (int)std::count_if(midi.bindings.begin(), midi.bindings.end(), [&](const MidiBinding& b) { return b.device == device; }));
}

