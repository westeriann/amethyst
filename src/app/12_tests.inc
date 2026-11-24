// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).

void writePng(const std::string& path, int w, int h, std::vector<unsigned char>& rgba) {
    static uint32_t crcTab[256];
    if (!crcTab[1])
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crcTab[n] = c;
        }
    auto crc = [&](const unsigned char* d, size_t n) {
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < n; i++) c = crcTab[(c ^ d[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    std::vector<unsigned char> raw;
    raw.reserve((size_t)(w * 4 + 1) * h);
    for (int y = h - 1; y >= 0; y--) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + (size_t)y * w * 4, rgba.begin() + (size_t)(y + 1) * w * 4);
    }
    std::vector<unsigned char> z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size(); off += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        z.push_back(off + n == raw.size());
        z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
    }
    uint32_t ad = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) z.push_back((ad >> s) & 0xFF);
    std::ofstream f(fs::u8path(path), std::ios::binary);
    auto be = [&](uint32_t v) { unsigned char c[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v}; f.write((char*)c, 4); };
    auto chunk = [&](const char* type, const std::vector<unsigned char>& d) {
        be((uint32_t)d.size());
        std::vector<unsigned char> td(type, type + 4);
        td.insert(td.end(), d.begin(), d.end());
        f.write((char*)td.data(), td.size());
        be(crc(td.data(), td.size()));
    };
    f.write("\x89PNG\r\n\x1a\n", 8);
    std::vector<unsigned char> ihdr = {(unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8), (unsigned char)w,
                                       (unsigned char)(h >> 24), (unsigned char)(h >> 16), (unsigned char)(h >> 8), (unsigned char)h, 8, 6, 0, 0, 0};
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
}

void captureBackbuffer(const std::string& name, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
    writePng(selftestDir + "\\" + name + ".png", w, h, px);
    fprintf(stderr, "[selftest] %s\n", name.c_str());
}

void pressKey(SDL_Keycode k, Uint16 mod = 0) {
    SDL_KeyboardEvent e{};
    e.type = SDL_KEYDOWN;
    e.windowID = SDL_GetWindowID(win);
    e.keysym.sym = k;
    e.keysym.scancode = SDL_GetScancodeFromKey(k);
    e.keysym.mod = mod;
    handleKey(e, ImGui::GetIO().WantTextInput);
}

struct TestStep { double at; std::function<void()> act; };
std::vector<TestStep> testSteps;
size_t testNext = 0;
double testStart = 0;
std::string pendingShot, pendingOutShot;
std::ofstream testLog;
void testNote(const std::string& s) { testLog << s << std::endl; }
std::string curName(int d) { return decks[d].cur >= 0 ? lib[decks[d].cur].name : "-"; }

UdpProbe artProbe;

// ---- perf test: preset load cost and phone-remote latency (VortexVJ.exe --perftest <dir> --add-folder <presets>)
bool perfMode = false;
std::vector<double> perfFrames;
std::string perfStats(std::vector<double> v) {
    if (v.empty()) return "n=0";
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (double x : v) sum += x;
    int over = (int)std::count_if(v.begin(), v.end(), [](double x) { return x > 40; });
    char b[160];
    snprintf(b, sizeof b, "n=%d avg=%.1f p50=%.1f p95=%.1f max=%.1f ms, >40ms: %d", (int)v.size(), sum / v.size(), v[v.size() / 2],
             v[std::min(v.size() - 1, v.size() * 95 / 100)], v.back(), over);
    return b;
}
std::vector<double> perfLoads, perfHttpA, perfHttpState;
size_t perfMark = 0;
std::atomic<bool> perfHttpDone{false};

void buildPerfTest() {
    double t = 2.5;
    auto at = [&](double dt, std::function<void()> f) { t += dt; testSteps.push_back({t, std::move(f)}); };
    at(0, [] {
        if (getenv("AMETHYST_PERF_NO_ANALYSIS")) { S.analysisOn = false; thumbs.analysisEnabled = false; }
        testNote("presets=" + std::to_string(lib.size()) + " render=" + std::to_string(renderW) + "x" + std::to_string(renderH) + " analysis=" + std::to_string(S.analysisOn));
        perfMark = perfFrames.size();
    });
    at(2.0, [] { testNote("idle frames: " + perfStats(std::vector<double>(perfFrames.begin() + perfMark, perfFrames.end()))); perfMark = perfFrames.size(); });
    for (int i = 0; i < 16; i++)
        at(0.4, [i] {
            int idx = pickRandom(decks[0].cur);
            double t0 = now();
            loadPreset(0, idx, i % 2 == 1);
            perfLoads.push_back((now() - t0) * 1000.0);
        });
    at(0.5, [] {
        testNote("loadPreset (main thread): " + perfStats(perfLoads));
        testNote("frames during clicks: " + perfStats(std::vector<double>(perfFrames.begin() + perfMark, perfFrames.end())));
        S.webOn = true;
        applyNetSettings();
        perfMark = perfFrames.size();
        std::thread([] {
            for (int i = 0; i < 40; i++) {
                perfHttpA.push_back(httpRequestMs(S.webPort, "/a/fx_kaleido", "POST"));
                perfHttpState.push_back(httpRequestMs(S.webPort, "/state"));
                Sleep(40);
            }
            perfHttpDone = true;
        }).detach();
    });
    at(6.0, [] {
        testNote(std::string("remote done=") + (perfHttpDone ? "yes" : "no"));
        testNote("remote /a/fx_kaleido: " + perfStats(perfHttpA));
        testNote("remote /state: " + perfStats(perfHttpState));
        testNote("frames during remote: " + perfStats(std::vector<double>(perfFrames.begin() + perfMark, perfFrames.end())));
    });
    at(0.5, [] { running = false; });
}

void testRenderStart() {  // render 10 s of a synthetic 128 BPM track to a 640x360 video with audio
        std::vector<float> pcm(48000 * 10 * 2);
        for (size_t i = 0; i < pcm.size() / 2; i++) {
            double tt = i / 48000.0, ph = std::fmod(tt, 60.0 / 128);
            float v = (float)(0.7 * std::sin(2 * M_PI * 55 * ph) * std::exp(-ph / 0.12) + 0.05 * std::sin(2 * M_PI * 440 * tt));
            pcm[i * 2] = pcm[i * 2 + 1] = v;
        }
        renderTestOut = selftestDir + "\\render-test.mp4";
        bool ok = startRenderJob(std::move(pcm), "synthetic", renderTestOut, 640, 360, 30, true);
        testNote(std::string("render started=") + (ok ? "yes" : "no") + " prescan bpm=" + std::to_string(rj.bpm));
    }
void testRenderCheck() {  // virtual seconds: the clock follows the render
        std::error_code ec;
        auto size = fs::file_size(fs::u8path(renderTestOut), ec);
        testNote("render done: active=" + std::to_string(rj.active) + " frames=" + std::to_string(rj.lastFrames) + " (expected 330) video secs=" +
                 std::to_string(rj.lastSecs) + " wall=" + std::to_string(rj.lastWall) + " size=" + std::to_string(ec ? 0 : size) + " " + rj.lastErr);
    }
bool renderTestOnly = false;
int hotBase = 0;
void buildSelftest() {
    double t = 2.5;
    auto at = [&](double dt, std::function<void()> f) { t += dt; testSteps.push_back({t, std::move(f)}); };
    if (renderTestOnly) {
        at(0.5, [] { testRenderStart(); });
        at(12.5, [] { testRenderCheck(); });
        at(0.5, [] { running = false; });
        return;
    }
    at(0, [] { testNote("presets=" + std::to_string(lib.size()) + " A=" + curName(0) + " audio=" + audio.label() + " sources=" + std::to_string(sourceNames.size())); pendingShot = "01-start"; });
    // smart auto-mix: synthetic 128 BPM tracks -> groove 16 bars, breakdown 8 bars, drop 16 bars, silence;
    // the second one has a fakeout: one bar of drop, one bar without bass, then the real drop at beat 104
    auto synth = [](std::function<bool(double)> kickOn, double beats) {
        MusicStructure ms;
        const double rate = 48000, bpm = 128, spb = 60 / bpm, t0 = 1.0;
        const double total = t0 + beats * spb + 3.0;
        std::vector<float> blk(480 * 2);
        std::mt19937 g(7);
        std::uniform_real_distribution<float> nz(-1, 1);
        std::string log;
        for (size_t f0 = 0; f0 < (size_t)(total * rate); f0 += 480) {
            for (int i = 0; i < 480; i++) {
                double tt = (f0 + i) / rate, sb = (tt - t0) / spb;
                float v = 0;
                if (sb >= 0 && sb < beats) {
                    double ph = (sb - std::floor(sb)) * spb;
                    if (kickOn(sb)) v += 0.7f * (float)(std::sin(2 * M_PI * 55 * ph) * std::exp(-ph / 0.12));
                    v += 0.08f * (float)std::sin(2 * M_PI * 440 * tt);                       // pad
                    if (sb >= 80 && sb < 96) v += 0.02f * (float)(sb - 80) * nz(g) * 0.3f;  // build: rising noise
                    else v += 0.03f * nz(g) * (float)std::exp(-std::fmod(ph, spb / 2) / 0.03);  // hats
                }
                blk[i * 2] = blk[i * 2 + 1] = v;
            }
            double tt = f0 / rate;
            ms.feed(blk.data(), 480, tt, (tt - t0) / spb, bpm);
            for (auto& e : ms.take()) {
                char b[96];
                snprintf(b, sizeof b, "%s@%.2f ", MusicStructure::name(e.type), e.beat);
                log += b;
            }
        }
        return log;
    };
    at(0, [synth] {
        testNote("structure: " + synth([](double b) { return b < 64 || b >= 96; }, 160));
        testNote("structure fakeout: " + synth([](double b) { return b < 64 || (b >= 96 && b < 100) || b >= 104; }, 168));
    });
    at(0, [] {  // auto-pilot stays inside the filtered list, even with a stale pre-picked preset
        query = "mashup";
        rebuildView();
        int outside = -1;
        for (int i = 0; i < (int)lib.size() && outside < 0; i++)
            if (std::find(view.begin(), view.end(), i) == view.end()) outside = i;
        upcoming = outside;
        int ok = 0;
        for (int k = 0; k < 10; k++) {
            go(0, 1, false, true);
            int p = decks[0].pendingIdx;
            if (p >= 0 && std::find(view.begin(), view.end(), p) != view.end()) ok++;
            upcoming = outside;
        }
        testNote("auto stays in filter: " + std::to_string(ok) + "/10 (view " + std::to_string(view.size()) + ")");
        query.clear();
        rebuildView();
    });
    // decks + crossfader
    at(1.0, [] { pressKey(SDLK_TAB); pressKey(SDLK_r); testNote("focus=" + std::string(decks[focus].name) + " B=" + curName(1)); S.showMixer = true; });
    at(0.5, [] { actions.invoke("crossfade", true, true, 0.5f); S.mixMode = MixScreen; });
    at(1.5, [] { pendingShot = "02-mixer-screen-50"; });
    at(0.5, [] { pressKey(SDLK_BACKSLASH); testNote("auto crossfade from 0.5"); });
    at(3.0, [] { testNote("xfade after auto=" + std::to_string(S.xfade)); pressKey(SDLK_TAB); S.showMixer = false; S.mixMode = 0; });
    // FX
    at(0.5, [] { pressKey(SDLK_F2); pressKey(SDLK_F4); pressKey(SDLK_F5); S.showFx = true; });
    at(1.5, [] { pendingShot = "03-fx-kaleido-trails-rgb"; });
    at(0.5, [] { actions.invoke("fx_reset", true); S.showFx = false; });
    // overlays: text
    at(0.3, [] { S.text = "AMETHYST"; S.textOn = true; S.textPos = 1; S.textSize = 0.15f; S.showOverlays = true; });
    at(1.0, [] { pendingShot = "04-text-overlay"; });
    at(0.5, [] { S.textOn = false; S.showOverlays = false; });
    // library meta + setlist
    at(0.3, [] {
        pressKey(SDLK_d, KMOD_LCTRL); pressKey(SDLK_4, KMOD_LCTRL); pressKey(SDLK_l, KMOD_LCTRL);
        if (decks[0].cur < 0) { testNote("library empty - skipping meta checks"); return; }
        Meta& m = metaOf(decks[0].cur);
        testNote("meta fav=" + std::to_string(m.fav) + " rating=" + std::to_string(m.rating) + " setlists=" + std::to_string(setlists.size()));
        setPool("fav");
        testNote("fav pool size=" + std::to_string(view.size()));
        setPool("all");
    });
    // tempo
    at(0.3, [] { for (int i = 0; i < 4; i++) { tempo.tap(now() + i * 0.5); } testNote("tap bpm=" + std::to_string(tempo.bpm()) + " src=" + tempo.sourceName()); tempo.setSource(Tempo::Source::Auto); });
    // quantized launch
    at(0.3, [] { S.quantize = 1; S.quantizeManual = true; std::string before = curName(0); pressKey(SDLK_RIGHT); testNote(std::string("launch armed=") + (launch.armed ? "yes" : "no") + " before=" + before); });
    at(1.5, [] { testNote("after quantized launch A=" + curName(0) + " armed=" + (launch.armed ? "yes" : "no")); S.quantizeManual = false; });
    // outputs + mapping + spout + record
    at(0.3, [] { Output* o = openOutput(-1, 0); if (o) { float c[8] = {0.1f, 0.05f, 0.95f, 0.15f, 0.85f, 0.9f, 0.05f, 0.95f}; memcpy(o->c, c, sizeof c); o->grid = true; } S.showOutputs = true; setSpout(true); startRecording(); });
    at(2.0, [] { pendingOutShot = "05-output-cornerpin-grid"; pendingShot = "06-outputs-panel"; });
    at(1.5, [] { testNote(std::string("recording=") + (recorder.active() ? "yes" : "no") + " secs=" + std::to_string(recorder.seconds()) + " dropped=" + std::to_string(recorder.dropped())); stopRecording(); setSpout(false); closeOutput(0); S.showOutputs = false; });
    // network
    at(0.3, [] { S.webOn = true; S.oscOn = true; applyNetSettings(); testNote(std::string("web=") + (web.running() ? "on" : "off") + " osc=" + (osc.running() ? "on" : "off")); S.showControl = true; });
    at(1.0, [] { pendingShot = "07-control-panel"; });
    // editor
    at(0.5, [] { S.showControl = false; openEditor(); testNote("editor chars=" + std::to_string(editorText.size())); });
    at(1.0, [] { pendingShot = "08-editor"; });
    at(0.3, [] {
        MilkDoc d;
        d.parse(editorText);
        bool rt = d.text() == editorText;
        auto eq = d.perFrame();
        eq.push_back("zoom = zoom + 0.08*(bass_att-1);");
        d.setPerFrame(eq);
        d.set("fDecay", 0.9, false);
        MilkDoc d2;
        d2.parse(d.text());
        testNote("visual editor roundtrip=" + std::string(rt ? "ok" : "FAIL") + " perframe=" + std::to_string(d2.perFrame().size()) +
                 "/" + std::to_string(eq.size()) + " decay=" + std::to_string(d2.get("fDecay", -1)));
        editorText = d.text();
        visualEditor.external(editorText);
        applyEditor();
    });
    at(1.0, [] { testNote("visual editor apply: " + editorStatus); pendingShot = "08b-editor-visual"; });
    at(0.5, [] {
        editorOpen = false;
        keyboardOpen = true;
        bindKey(SDLK_y, 0, "blackout");          // remap: Y -> Blackout
        bindKey(SDLK_b, 0, "");                  // and free up B
        pressKey(SDLK_y);
        testNote(std::string("after remap, Y -> blackout=") + (blackoutOn ? "on" : "off"));
        pressKey(SDLK_b);
        testNote(std::string("B unbound, blackout still=") + (blackoutOn ? "on" : "off"));
        pressKey(SDLK_y);
        testNote("help chords for blackout: " + (chordsFor("blackout").empty() ? std::string("-") : chordsFor("blackout")[0]));
    });
    at(1.0, [] { pendingShot = "09-keyboard-editor"; });
    at(0.5, [] { keyboardOpen = false; helpOpen = true; });
    at(1.0, [] { pendingShot = "10-help-live"; });
    at(0.5, [] { helpOpen = false; pressKey(SDLK_u); });
    at(1.0, [] { pendingShot = "11-ui-hidden"; });
    at(0.5, [] {
        pressKey(SDLK_u);
        int broken = 0;
        for (auto& p : lib) broken += p.broken;
        testNote("fps=" + std::to_string((int)ImGui::GetIO().Framerate) + " broken=" + std::to_string(broken) + " render=" + std::to_string(renderW) + "x" + std::to_string(renderH));
        pendingShot = "12-final";
    });
    // ---- DJ link against the built-in fake Engine OS device
    at(0.5, [] {
        static std::atomic<bool> fakeStop{false};
        StageLinq::startFakeDevice(fakeStop);
        S.djTrackAction = 2;
        S.djShowTitle = true;
        setDjLink(true);
    });
    at(4.0, [] {
        testNote("dj status=" + djSnap.status + " | device=" + djSnap.device);
        if (!djSnap.decks.empty()) {
            const DjDeck& d = djSnap.decks[0];
            testNote("dj deck1 title=" + d.title + " artist=" + d.artist + " bpm=" + std::to_string(d.bpm) + " beat=" + std::to_string(d.beat) + " playing=" + std::to_string(d.playing));
        }
        if (djSnap.decks.size() > 1) testNote("dj deck2 title=" + djSnap.decks[1].title + " bpm=" + std::to_string(djSnap.decks[1].bpm) + " playing=" + std::to_string(djSnap.decks[1].playing));
        testNote(std::string("tempo src=") + tempo.sourceName() + " bpm=" + std::to_string(tempo.bpm()));
    });
    at(2.5, [] { testNote("dj last title=" + djLastTitle + " | overlay=" + djTitleText + " | A=" + curName(0)); pendingShot = "13-dj-title"; S.showControl = true; selectDjTab = true; });
    at(0.7, [] { pendingShot = "14-dj-panel"; });
    // ---- scenes, morphing, show timeline
    at(0.5, [] {
        S.showControl = false;
        setDjLink(false);
        tempo.setSource(Tempo::Source::Tap);
        tempo.setBpm(240, now());
        S.fx = FxParams{};
        storeScene(0);
        S.fx.kaleido = true;
        S.fx.hue = 0.5f;
        go(0, 1, true);
        storeScene(1);
        S.sceneMorphBeats = 2;
        recallScene(0);
        testNote("scene recall: morph active=" + std::to_string(morph.active) + " A=" + curName(0));
    });
    at(1.0, [] {
        testNote("after morph: kaleido=" + std::to_string(S.fx.kaleido) + " hue=" + std::to_string(S.fx.hue) + " morph active=" + std::to_string(morph.active));
        timeline = {{0, 1}, {1, 1}};
        tlLoop = true;
        timelinePlay(true);
        S.showScenes = true;
    });
    at(1.6, [] { testNote("timeline step=" + std::to_string(tlIndex) + " active scene=" + std::to_string(activeScene) + " (1 bar at 240 BPM = 1 s)"); pendingShot = "15-scenes"; });
    // ---- modulation
    at(0.5, [] {
        timelinePlay(false);
        S.showScenes = false;
        S.fx = FxParams{};
        mods.clear();
        ModRoute r; r.src = SrcLfoSine; r.dst = DstHue; r.amount = 1; r.rate = 1; r.smooth = 0; mods.push_back(r);
        ModRoute z; z.src = SrcLfoSquare; z.dst = DstZoom; z.amount = 0.5f; z.rate = 2; z.smooth = 0; mods.push_back(z);
        S.showMod = true;
    });
    at(0.8, [] { testNote("mod: hue base=" + std::to_string(S.fx.hue) + " eff=" + std::to_string(modOut.fx.hue) + " | zoom base=" + std::to_string(S.fx.zoom) + " eff=" + std::to_string(modOut.fx.zoom)); pendingShot = "16-modulation"; });
    // ---- transition
    at(0.5, [] { mods.clear(); S.showMod = false; S.transition = TransWipe; S.xfade = 0; go(1, 1, true, true); startXfade(1.f, 2.0); });
    at(1.0, [] { testNote("transition wipe mid xfade=" + std::to_string(S.xfade) + " anim=" + std::to_string(xanim.active) + " from=" + std::to_string(xanim.from) + " to=" + std::to_string(xanim.to) + " elapsed=" + std::to_string(now() - xanim.start) + " dur=" + std::to_string(xanim.dur) + " morph=" + std::to_string(morph.active) + " tl=" + std::to_string(tlPlaying)); pendingShot = "17-transition-wipe"; });
    // ---- Art-Net loopback
    at(1.5, [] {
        bool ok = artProbe.open(6454);
        testNote(std::string("artnet probe bound=") + (ok ? "yes" : "no"));
        S.dmxOn = true; S.dmxIp = "127.0.0.1"; S.dmxFixtures = 8; S.dmxMode = 3; S.dmxStart = 1;
        S.showOutputs = true;
    });
    at(0.6, [] {
        std::vector<unsigned char> pkt, last;
        int count = 0;
        while (artProbe.receive(pkt)) { count++; last = pkt; }
        artProbe.close();
        bool header = last.size() >= 18 && memcmp(last.data(), "Art-Net\0", 8) == 0 && last[8] == 0x00 && last[9] == 0x50;
        int len = last.size() >= 18 ? (last[16] << 8 | last[17]) : 0;
        char rgb[64] = "";
        if (last.size() >= 23) snprintf(rgb, sizeof rgb, " fixture1 dim=%d rgb=%d,%d,%d", last[18], last[19], last[20], last[21]);
        testNote("artnet received " + std::to_string(count) + " packets, header ok=" + std::to_string(header) + " dmx length=" + std::to_string(len) + rgb +
                 " (sent " + std::to_string(artnet.sent) + ")");
        pendingShot = "18-lighting";
    });
    // ---- live inputs
    at(0.4, [] {
        S.dmxOn = false;
        testNote("cameras=" + std::to_string(VideoPlayer::cameras().size()));
        setSpout(true);
        S.layerSource = 2;
        S.spoutInput = "Amethyst";
        S.videoOn = true;
        S.videoOpacity = 0.5f;
        S.showOutputs = false;
        S.showOverlays = true;
    });
    at(1.5, [] { testNote("spout input " + std::to_string(spoutRxW) + "x" + std::to_string(spoutRxH) + " from '" + spoutRxConnected + "'"); pendingShot = "19-spout-input"; });
    // ---- smart library + MIDI template
    at(0.4, [] {
        setSpout(false);
        S.videoOn = false;
        S.showOverlays = false;
        testNote("analysed=" + std::to_string(looks.size()) + " pending=" + std::to_string(thumbs.analysisPending()));
        std::map<std::string, int> colors;
        for (auto& [p, l] : looks) colors[colorOf(l)]++;
        std::string cs;
        for (auto& [c, n] : colors) cs += c + "=" + std::to_string(n) + " ";
        testNote("colours: " + cs);
        if (!looks.empty()) {
            setPool("similar:" + looks.begin()->first);
            testNote("similar pool size=" + std::to_string(view.size()));
            setPool("all");
        }
        applyTemplate(1, "Launchpad X");
        int n = 0;
        for (auto& b : midi.bindings) n += b.device == "Launchpad X";
        testNote("launchpad template bindings=" + std::to_string(n) + " style=" + std::to_string(midi.feedbackStyle["Launchpad X"]));
        S.showBrowser = true;
        setPool("color:Blue");
    });
    at(1.0, [] { pendingShot = "20-colour-pool"; setPool("all"); });
    // Auto FX: pin the song section and look at what the effects do
    at(0.5, [] {
        S.autoOn = true, S.autoFx = true, S.autoFxAmount = 0.8f, S.showMixer = false;
        structure.debugPin(MusicStructure::Build, beatNow() - 28);
    });
    at(3.0, [] {  // the drop preset should have been loaded and be teasing in
        Deck& dk = decks[smart.deck < 0 ? 0 : smart.deck];
        testNote("build tease: prep=" + std::to_string(smart.prep) + " peeking=" + std::to_string(smart.peeking) +
                 " amount=" + std::to_string(S.autoDecks ? S.xfade : dk.blendManual));
        pendingShot = "20b-build-tease";
    });
    at(1.2, [] {
        testNote("autofx build: zoom=" + std::to_string(modOut.fx.zoom) + " swirl=" + std::to_string(modOut.fx.swirl) +
                 " rgb=" + std::to_string(modOut.fx.rgbSplitAmount));
        pendingShot = "21-autofx-build";
    });
    at(0.5, [] {
        structure.debugPin(MusicStructure::Drop, beatNow());
        structure.debugEmit(MusicStructure::DropHit, beatNow(), now());
    });
    at(0.1, [] {
        testNote("autofx drop: look=" + std::string(kLookNames[afx.look]) + " zoom=" + std::to_string(modOut.fx.zoom) +
                 " flash=" + std::to_string(flashAmt) + " transition=" + std::to_string(afx.transition));
        Deck& dk = decks[smart.deck < 0 ? 0 : smart.deck];
        testNote("drop cut: blending=" + std::to_string(dk.blending) + " manual=" + std::to_string(dk.blendManual) + " prep=" + std::to_string(smart.prep));
        pendingShot = "22-autofx-drop";
    });
    at(1.5, [] { pendingShot = "23-autofx-drop-later"; });
    at(0.5, [] {
        structure.debugPin(MusicStructure::Silence, beatNow(), false);
        S.autoOn = false;
        int n = 0;
        std::vector<float> p;
        for (auto& [k, l] : looks)
            if (l.punch >= 0) p.push_back(l.punch);
        std::sort(p.begin(), p.end());
        auto q = [&](float f) { return p.empty() ? -1.f : p[std::min(p.size() - 1, (size_t)(f * p.size()))]; };
        for (auto& [k, l] : looks) n += energyClass(l) > 0;
        wdTestMode = true;
        S.watchdog = true;
        structure.debugPin(MusicStructure::Groove, beatNow());
        wdBefore = decks[onAirDeck()].cur;
        S.fx.brightness = -1;  // force the picture to black
        testNote("punch measured=" + std::to_string(p.size()) + " p10=" + std::to_string(q(0.1f)) + " p50=" + std::to_string(q(0.5f)) +
                 " p90=" + std::to_string(q(0.9f)) + " intense=" + std::to_string(n));
    });
    at(10.0, [] {
        int now_ = decks[onAirDeck()].cur;
        testNote("watchdog: before=" + std::to_string(wdBefore) + " after=" + std::to_string(now_) +
                 " before broken=" + std::to_string(wdBefore >= 0 && lib[wdBefore].broken));
        S.fx = FxParams{};
        wdTestMode = false;
        structure.debugPin(MusicStructure::Silence, beatNow(), false);
        S.safePreset = lib[decks[onAirDeck()].cur].path;
    });
    at(0.5, [] { go(onAirDeck(), 1, true, true); });
    at(0.5, [] { testRenderStart(); });
    at(13.0, [] { testRenderCheck(); });  // virtual seconds: the clock follows the render
    at(0.5, [] {  // custom shaders: every bundled one must compile
        scanShaders();
        int ok = 0;
        std::string bad;
        for (auto& u : userShaders) {
            if (ensureShader(u)) ok++;
            else bad += " " + u.name + ": " + u.err.substr(0, 120);
        }
        testNote("shaders compiled " + std::to_string(ok) + "/" + std::to_string(userShaders.size()) + bad);
        S.userShader = "kaleido-tunnel";
        S.userShaderAmount = 1;
    });
    at(1.0, [] { pendingShot = "25-shader-kaleido-tunnel"; S.userShader = "edge-neon"; });
    at(1.0, [] { pendingShot = "26-shader-edge-neon"; S.userShader.clear(); });
    at(0.2, [] {  // hot reload: write a shader, break it, fix it - the running effect follows the file
        std::ofstream(fs::u8path(userShaderDir() + "\\zz-hot.glsl")) << "void mainImage(out vec4 c, in vec2 p) { c = vec4(1.0, 0.0, 0.0, 1.0); }\n";
        scanShaders();
        S.userShader = "zz-hot";
        S.userShaderAmount = 1;
        hotBase = shaderCompiles;
    });
    at(1.5, [] {
        testNote("hot reload 1 (red): compiled=" + std::to_string(shaderCompiles - hotBase));
        std::ofstream(fs::u8path(userShaderDir() + "\\zz-hot.glsl")) << "void mainImage(out vec4 c, in vec2 p) { c = vec4(0.0, 1.0, 0.0, 1.0) }\n";  // missing ;
        std::error_code ec;  // make sure the timestamp moves even on coarse file systems
        fs::last_write_time(fs::u8path(userShaderDir() + "\\zz-hot.glsl"), fs::file_time_type::clock::now() + std::chrono::seconds(2), ec);
    });
    at(2.0, [] {
        int i = shaderIndex("zz-hot");
        testNote("hot reload 2 (broken): error shown=" + std::string(i >= 0 && !userShaders[i].err.empty() ? "yes" : "no") +
                 " still running old=" + std::string(i >= 0 && userShaders[i].prog ? "yes" : "no"));
        std::ofstream(fs::u8path(userShaderDir() + "\\zz-hot.glsl")) << "void mainImage(out vec4 c, in vec2 p) { c = vec4(0.0, 1.0, 0.0, 1.0); }\n";
        std::error_code ec;
        fs::last_write_time(fs::u8path(userShaderDir() + "\\zz-hot.glsl"), fs::file_time_type::clock::now() + std::chrono::seconds(4), ec);
    });
    at(2.0, [] {
        int i = shaderIndex("zz-hot");
        testNote("hot reload 3 (fixed): compiled=" + std::to_string(shaderCompiles - hotBase) + " error=" + (i >= 0 ? userShaders[i].err : std::string("?")));
        pendingShot = "27-shader-hot-reload-green";
    });
    at(0.5, [] { S.userShader.clear(); });
    at(0.5, [] {  // stay-in-folder lock
        buildAutoFolders();
        std::string pick;
        for (auto& [label, path] : autoFolders)
            if (label.find("/") != std::string::npos && label.find("Dancer") != std::string::npos) { pick = path; break; }
        if (pick.empty() && autoFolders.size() > 1) pick = autoFolders[1].second;
        setAutoFolder(pick);
        int inside = 0;
        for (int k = 0; k < 10; k++) {
            int a = smartPick(0);
            if (a >= 0 && inAutoFolder(a)) inside++;
            AutoPickScope scope;
            int b = pickRandom(decks[0].cur);
            if (b >= 0 && inAutoFolder(b)) inside++;
        }
        testNote("auto folder lock: " + autoFolderLabel() + " pool=" + std::to_string(autoView.size()) + " inside=" + std::to_string(inside) + "/20" +
                 " folders=" + std::to_string(autoFolders.size()));
        S.showStatus = true;
        autoMenuTestOpen = true;
    });
    at(0.8, [] { pendingShot = "24-auto-menu"; });
    at(0.5, [] { setAutoFolder(""); ImGui::CloseCurrentPopup(); });
    at(2.0, [] {
        int before = decks[onAirDeck()].cur;
        panicSafe();
        wdBefore = before;
    });
    at(2.0, [] {
        testNote("panic: safe=" + std::string(lib[decks[onAirDeck()].cur].path == S.safePreset ? "yes" : "no") + " from=" + std::to_string(wdBefore));
    });
    at(1.0, [] { running = false; });
}
