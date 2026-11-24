// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== self-test
std::string selftestDir;
std::string gDataDir;  // where settings and the session file live
bool wdTestMode = false, recoveredStart = false;
std::string crashTestDir;  // --crashtest: crash on purpose and check the automatic restart
int wdBefore = -1;
// ===================================================================== show safety
// A watchdog samples the output twice a second (async readback, no stall). If music is playing but
// the picture has gone black or stopped moving for 8 s, the preset is skipped for the safe preset;
// if that does not help, the app restarts itself. Presets that drag the frame rate down get skipped.
// A session file remembers what was loading, so a preset that crashed the app is marked broken.
std::string sessionPath() { return gDataDir + "\\session.txt"; }
std::string crashedPreset;  // set at startup when the last session ended in a crash
bool lastRunCrashed = false;
void sessionMark(const std::string& loading) {
    if (!selftestDir.empty()) return;
    std::ofstream f(fs::u8path(sessionPath()), std::ios::trunc);
    f << "running\n" << loading << "\n";
}
void sessionCheckAtStart() {
    std::ifstream f(fs::u8path(sessionPath()));
    std::string a;
    if (!std::getline(f, a)) return;
    lastRunCrashed = true;
    std::getline(f, crashedPreset);
}

int safeIndex(int avoid) {
    auto it = byPath.find(S.safePreset);
    if (it != byPath.end() && !lib[it->second].broken && it->second != avoid) return it->second;
    for (int i : view)  // otherwise the first healthy preset in the bundled folder or the list
        if (!lib[i].broken && i != avoid && lib[i].path.find(plat::exeDir()) == 0) return i;
    return pickRandom(avoid);
}
void goSafe(int d, const char* why, bool markBad) {
    int cur = decks[d].cur;
    if (markBad && cur >= 0) markBroken(cur, why);
    int idx = safeIndex(cur);
    if (idx >= 0) loadPreset(d, idx, true);
    if (smart.prep) smartCancel();
    if (cur >= 0) toast(true, "Show safety: \"%s\" %s, switched to a safe preset", lib[cur].name.c_str(), why);
}
void panicSafe() {  // the panic button: safe preset on air, effects reset, no flashes or blackout
    S.fx = FxParams{};
    flashAmt = 0, blackoutOn = false, frozen = false;
    afx = AutoFx{};
    for (int d = 0; d < 2; d++) decks[d].blending = false, decks[d].blendManual = -1;
    goSafe(onAirDeck(), "was replaced by the panic button", false);
}

struct Watchdog {
    RenderTarget rt;
    unsigned pbo[2] = {0, 0};
    int idx = 0;
    bool primed = false;
    double lastSample = 0, darkSince = -1, stillSince = -1, fallbackAt = -100, slowSince = -1;
    std::vector<float> prev;
    float dtAvg = 0.016f;
    std::unordered_map<std::string, int> slowStrikes;
} wd;

void watchdogTick(double t, double dt) {
    wd.dtAvg += ((float)dt - wd.dtAvg) * 0.05f;
    if (!S.watchdog || !selftestDir.empty() && !wdTestMode) return;
    int on = onAirDeck();
    Deck& dk = decks[on];
    bool loading = anyLoading() || dk.blending || xanim.active;
    // too heavy: under ~14 fps for 4 s with nothing else going on
    if (wd.dtAvg > 0.07f && !loading && t - dk.lastSwitch > 4) {
        if (wd.slowSince < 0) wd.slowSince = t;
        if (t - wd.slowSince > 4 && dk.cur >= 0) {
            wd.slowSince = -1;
            int strikes = ++wd.slowStrikes[lib[dk.cur].path];
            toast(true, "Show safety: \"%s\" is too heavy for this computer, skipping it", lib[dk.cur].name.c_str());
            if (strikes >= 2) markBroken(dk.cur, "too slow");
            go(on, 1, true, true);
        }
    } else {
        wd.slowSince = -1;
    }
    // output sample, twice a second
    if (t - wd.lastSample < 0.5) return;
    wd.lastSample = t;
    const int W = 32, H = 18;
    if (!wd.pbo[0]) glGenBuffers(2, wd.pbo);
    wd.rt.ensure(W, H);
    glBindFramebuffer(GL_FRAMEBUFFER, wd.rt.fbo);
    glViewport(0, 0, W, H);
    gfx.present(finalRT.tex, 0, 0, W, H, nullptr, false);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, wd.pbo[wd.idx]);
    glBufferData(GL_PIXEL_PACK_BUFFER, W * H * 4, nullptr, GL_STREAM_READ);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    std::vector<float> lum;
    if (wd.primed) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, wd.pbo[1 - wd.idx]);
        if (auto* p = (const unsigned char*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, W * H * 4, GL_MAP_READ_BIT)) {
            lum.resize(W * H);
            for (int i = 0; i < W * H; i++) lum[i] = (p[i * 4] + p[i * 4 + 1] + p[i * 4 + 2]) / 765.f;
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
    }
    wd.primed = true;
    wd.idx = 1 - wd.idx;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (lum.empty()) return;
    double mean = 0, diff = 0;
    for (size_t i = 0; i < lum.size(); i++) {
        mean += lum[i];
        if (wd.prev.size() == lum.size()) diff += std::fabs(lum[i] - wd.prev[i]);
    }
    mean /= lum.size(), diff /= lum.size();
    wd.prev = lum;
    // only judge while music plays and nothing dims or holds the picture on purpose
    bool expectLife = (level > 0.01f || wdTestMode) && !blackoutOn && blackAmt < 0.05f && afx.black < 0.2f && !frozen && S.speed > 0.05f &&
                      structure.section() != MusicStructure::Silence && !loading;
    if (!expectLife) { wd.darkSince = wd.stillSince = -1; return; }
    if (mean < 0.015) { if (wd.darkSince < 0) wd.darkSince = t; } else wd.darkSince = -1;
    if (diff < 0.0015) { if (wd.stillSince < 0) wd.stillSince = t; } else wd.stillSince = -1;
    bool dark = wd.darkSince >= 0 && t - wd.darkSince > 8, still = wd.stillSince >= 0 && t - wd.stillSince > 8;
    if (!dark && !still) return;
    wd.darkSince = wd.stillSince = -1;
    if (t - wd.fallbackAt < 20 && still) {  // the safe preset did not help: the GPU or a player is stuck
        toast(true, "Show safety: output is stuck, restarting Amethyst");
        saveSettings();
        plat::restartSelf();
        return;
    }
    wd.fallbackAt = t;
    goSafe(on, dark ? "stayed dark" : "froze", !dark);  // dark can be on purpose: skip it, but only mark frozen ones broken
}

