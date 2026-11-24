// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== audio
void refreshDevices() { audioDevices = audio.devices(); }

void startAudioFrom(const std::string& kind, const std::string& device, bool quiet) {
    std::string err;
    bool ok = false;
    if (kind == "none") { audio.stop(); ok = true; }
    else if (kind == "file") ok = audio.startFile(device, err);
    else {
        bool loop = kind == "loopback";
        int idx = -1;
        for (auto& d : audioDevices)
            if (d.loopback == loop && d.name == device) idx = d.index;
        ok = loop ? audio.startLoopback(idx, err) : audio.startInput(idx, err);
    }
    if (ok) {
        S.audioKind = kind;
        S.audioDevice = device;
        saveSettings();
        if (!quiet) toast(false, "%s", audio.label().c_str());
    } else {
        toast(true, "Audio: %s", err.c_str());
    }
}

HWND hwndOf(SDL_Window* w) {
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    SDL_GetWindowWMInfo(w, &wm);
    return wm.info.win.window;
}

void playAudioFileDialog() {
    auto files = plat::pickFiles(hwndOf(win), L"Play an audio file", {{L"Audio", L"*.mp3;*.wav;*.flac;*.ogg"}}, false);
    if (!files.empty()) startAudioFrom("file", files[0], false);
}

void setGain(float g) {
    S.gain = std::clamp(g, 0.05f, 20.f);
    toast(false, "Sensitivity %.2fx", S.gain);
}

bool pumpAudio() {
    static std::vector<float> buf(48000 * 2);
    size_t n;
    if (rj.active) {  // the renderer hands over exactly one video frame of the file's audio
        if (!rj.feedNow || rj.chunk.empty()) return false;
        n = rj.chunk.size() / 2;
        std::copy(rj.chunk.begin(), rj.chunk.end(), buf.begin());
    } else {
        n = audio.read(buf.data(), 24000);
    }
    double t = now();
    if (n == 0) {
        level *= 0.9f;
        bandBass *= 0.9f; bandMid *= 0.9f; bandTreb *= 0.9f; bandLevel *= 0.9f;
        return false;
    }
    float* s = buf.data();
    static float lp = 0, lp2 = 0;
    double bassE = 0, allE = 0, midE = 0, trebE = 0;
    for (size_t i = 0; i < n; i++) {
        s[2 * i] *= S.gain;
        s[2 * i + 1] *= S.gain;
        float m = 0.5f * (s[2 * i] + s[2 * i + 1]);
        lp += (m - lp) * 0.02f;
        lp2 += (m - lp2) * 0.23f;  // ~2 kHz
        bassE += lp * lp;
        midE += (lp2 - lp) * (lp2 - lp);
        trebE += (m - lp2) * (m - lp2);
        allE += m * m;
    }
    float bass = (float)std::sqrt(bassE / n);
    {
        auto agc = [](float v, float& peak, float& out) {
            peak = std::max(v, std::max(0.01f, peak * 0.9985f));
            out = std::clamp(v / peak, 0.f, 1.f);
        };
        agc(bass, peakBass, bandBass);
        agc((float)std::sqrt(midE / n), peakMid, bandMid);
        agc((float)std::sqrt(trebE / n), peakTreb, bandTreb);
        agc((float)std::sqrt(allE / n), peakLevel, bandLevel);
    }
    level = std::max(level * 0.85f, (float)std::sqrt(allE / n));
    bool hit = bass > 0.03f && bass > bassAvg * 1.35f + 0.01f && t - lastBeatHit > 0.3;
    bassAvg += (bass - bassAvg) * 0.05f;
    if (hit) lastBeatHit = t;
    tempo.feed(s, n, t);
    structure.feed(s, n, t, tempo.beatAt(t), tempo.bpm());
    size_t maxS = projectm_pcm_get_max_samples();
    size_t off = n > maxS ? n - maxS : 0;
    for (auto& d : decks)
        for (auto& p : d.players) p.pushAudio(s + off * 2, n - off);
    mappingPushAudio(s + off * 2, n - off);
    thumbs.pushAudio(s, n);
    return hit;
}

// ===================================================================== outputs, spout, recording
void closeOutput(int i) {
    if (i < 0 || i >= (int)outputs.size()) return;
    SDL_DestroyWindow(outputs[i].win);
    outputs.erase(outputs.begin() + i);
    outputSel = std::clamp(outputSel, 0, std::max(0, (int)outputs.size() - 1));
    SDL_GL_MakeCurrent(win, ctx);
    toast(false, "Output closed");
    saveSettings();
}

Output* openOutput(int display, int source = 0, const float* corners = nullptr) {
    int n = SDL_GetNumVideoDisplays();
    int mine = SDL_GetWindowDisplayIndex(win);
    if (display == -2) {
        display = -1;
        for (int i = 0; i < n; i++)
            if (i != mine) { display = i; break; }
    }
    Output o;
    o.display = display;
    o.source = source;
    if (corners) memcpy(o.c, corners, sizeof o.c);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    char title[64];
    snprintf(title, sizeof title, "Amethyst - Output %d", (int)outputs.size() + 1);
    if (display >= 0 && display < n) {
        SDL_Rect b;
        SDL_GetDisplayBounds(display, &b);
        o.win = SDL_CreateWindow(title, b.x, b.y, b.w, b.h, flags | SDL_WINDOW_BORDERLESS);
        toast(false, "Output on %s", SDL_GetDisplayName(display));
    } else {
        o.win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags | SDL_WINDOW_RESIZABLE);
        toast(false, "Output window open — drag it to the projector, press F there for fullscreen");
    }
    SDL_GL_MakeCurrent(win, ctx);
    if (!o.win) { toast(true, "Could not open output: %s", SDL_GetError()); return nullptr; }
    outputs.push_back(o);
    outputSel = (int)outputs.size() - 1;
    saveSettings();
    return &outputs.back();
}

int outputIndexForWindow(Uint32 id) {
    for (size_t i = 0; i < outputs.size(); i++)
        if (SDL_GetWindowID(outputs[i].win) == id) return (int)i;
    return -1;
}

unsigned outputTexture(int source) {
    switch (source) {
        case 1: return decks[0].outTex;
        case 2: return decks[1].outTex;
        case 3: return mixRT.tex;
        case 4: return mappingTexture();
        default: return finalRT.tex;
    }
}

void toggleFullscreen(SDL_Window* w) {
    bool on = SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(w, on ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void setSpout(bool on) {
    S.spout = on;
    if (!on && spoutNamed) { spout.ReleaseSender(); spoutNamed = false; }
    toast(false, on ? "Spout sender \"Amethyst\" on — add a Spout source in OBS/Resolume" : "Spout off");
}

void startRecording() {
    if (recorder.active()) return;
    std::string dir = plat::videosDir() + "\\Amethyst";
    std::error_code ec;
    fs::create_directories(fs::u8path(dir), ec);
    char name[64];
    std::time_t tt = std::time(nullptr);
    std::strftime(name, sizeof name, "Amethyst-%Y%m%d-%H%M%S.mp4", std::localtime(&tt));
    std::string err;
    if (!recorder.start(dir + "\\" + name, finalRT.w, finalRT.h, S.recFps, err)) { toast(true, "Recording failed: %s", err.c_str()); return; }
    pboPrimed = false;
    recAccum = 0;
    toast(false, "Recording %dx%d @ %d fps -> Videos\\Amethyst", recorder.width(), recorder.height(), S.recFps);
}
void stopRecording() {
    if (!recorder.active()) return;
    std::string path = recorder.path();
    double secs = recorder.seconds();
    int dropped = recorder.dropped();
    recorder.stop();
    toast(false, "Saved %.1fs recording%s", secs, dropped ? " (some frames dropped)" : "");
    fprintf(stderr, "[Amethyst] recording saved: %s\n", path.c_str());
}

RenderTarget recRT;  // fixed recording size, so resizing outputs never interrupts a take

void captureForRecording(double dt) {
    if (!recorder.active() || rj.active) return;  // the file renderer captures its own frames
    recAccum += dt;
    double frame = 1.0 / S.recFps;
    if (recAccum < frame) return;
    int copies = std::min(4, (int)(recAccum / frame));
    recAccum -= copies * frame;
    const int w = recorder.width(), h = recorder.height();
    const size_t size = (size_t)w * h * 4;
    if (!pbo[0]) glGenBuffers(2, pbo);
    recRT.ensure(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, recRT.fbo);
    float a = finalRT.w / (float)std::max(1, finalRT.h), ra = w / (float)h;  // letterbox if aspect changed
    int vw = w, vh = h, vx = 0, vy = 0;
    if (a > ra) { vh = (int)(w / a); vy = (h - vh) / 2; } else { vw = (int)(h * a); vx = (w - vw) / 2; }
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx.present(finalRT.tex, vx, vy, vw, vh, nullptr, false);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, recRT.fbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pboIdx]);
    glBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STREAM_READ);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    if (pboPrimed) {  // map last frame's buffer (already finished) — no stall
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[1 - pboIdx]);
        if (void* p = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT)) {
            for (int c = 0; c < copies; c++) recorder.pushFrame((const unsigned char*)p);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
    }
    pboPrimed = true;
    pboIdx = 1 - pboIdx;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

