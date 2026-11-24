// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== overlays
void loadLogo() {
    if (S.logoPath == loadedLogoPath) return;
    loadedLogoPath = S.logoPath;
    if (S.logoPath.empty()) return;
    std::ifstream f(fs::u8path(S.logoPath), std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)), {});
    unsigned char* px = data.empty() ? nullptr : imageDecodeRGBA(data.data(), (int)data.size(), &logoW, &logoH);
    if (!px) { toast(true, "Could not load image %s", S.logoPath.c_str()); return; }
    if (!logoTex) glGenTextures(1, &logoTex);
    glBindTexture(GL_TEXTURE_2D, logoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, logoW, logoH, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    imageFree(px);
}

void openVideo(const std::string& path) {
    std::string err;
    if (video.open(path, err)) {
        S.videoPath = path;
        S.videoOn = true;
        videoSerial = -1;
        toast(false, "Video layer: %s", video.name().c_str());
        saveSettings();
    } else {
        toast(true, "Video: %s", err.c_str());
    }
}

void drawOverlays(RenderTarget& rt, double t) {
    rt.bind();
    const float W = (float)rt.w, H = (float)rt.h;
    auto px2ndc = [&](float x, float y) { return ImVec2(x / W * 2 - 1, 1 - y / H * 2); };
    // video layer
    if (S.videoOn && !rj.active && S.layerSource == 2 && receiveSpout()) {
        rt.bind();
        float va = spoutRxW / (float)std::max(1, spoutRxH), ra = W / H, sx = 1, sy = 1;
        if (S.videoFit == 0) { if (va > ra) sx = va / ra; else sy = ra / va; }
        else if (S.videoFit == 1) { if (va > ra) sy = ra / va; else sx = va / ra; }
        gfx.layer(spoutRxTex, -sx, -sy, sx, sy, modOut.videoOpacity, true, S.videoBlend);
    }
    rt.bind();
    if (S.videoOn && !rj.active && S.layerSource != 2 && video.active()) {
        int w, h;
        bool bu;
        if (video.latest(videoSerial, videoBuf, w, h, bu)) {
            if (!videoTex) glGenTextures(1, &videoTex);
            glBindTexture(GL_TEXTURE_2D, videoTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, videoBuf.data());
            videoW = w;
            videoH = h;
            videoBottomUp = bu;
        }
        if (videoTex && videoW > 0) {
            float va = videoW / (float)videoH, ra = W / H, sx = 1, sy = 1;
            if (S.videoFit == 0) { if (va > ra) sx = va / ra; else sy = ra / va; }       // fill
            else if (S.videoFit == 1) { if (va > ra) sy = ra / va; else sx = va / ra; }  // fit
            gfx.layer(videoTex, -sx, -sy, sx, sy, modOut.videoOpacity, !videoBottomUp, S.videoBlend);
        }
    }
    // logo
    if (S.logoOn && !S.logoPath.empty()) {
        loadLogo();
        if (logoTex && logoW > 0) {
            float w = S.logoScale * W, h = w * logoH / (float)logoW, m = 0.03f * std::min(W, H);
            float x = m, y = m;
            if (S.logoPos == 1 || S.logoPos == 3) x = W - w - m;
            if (S.logoPos == 2 || S.logoPos == 3) y = H - h - m;
            if (S.logoPos == 4) { x = (W - w) / 2; y = (H - h) / 2; }
            ImVec2 a = px2ndc(x, y + h), b = px2ndc(x + w, y);
            gfx.layer(logoTex, a.x, a.y, b.x, b.y, modOut.logoOpacity, true, BlendNormal);
        }
    }
    // text
    bool djTitle = t < djTitleUntil && !djTitleText.empty();
    if (((S.textOn && !S.text.empty()) || djTitle) && overlayCtx) {
        ImGui::SetCurrentContext(overlayCtx);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(W, H);
        io.DeltaTime = 1 / 60.f;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (djTitle) {  // "Artist - Title" from the DJ link, fading in and out
            float a = (float)std::min({1.0, (t - (djTitleUntil - S.djTitleSecs)) * 2, (djTitleUntil - t) * 1.5});
            float sz = 0.06f * H;
            ImVec2 ts = fontOverlay->CalcTextSizeA(sz, FLT_MAX, 0, djTitleText.c_str());
            float x = (W - ts.x) / 2, y = H * 0.05f, o = std::max(2.f, sz * 0.04f);
            dl->AddText(fontOverlay, sz, ImVec2(x + o, y + o), IM_COL32(0, 0, 0, (int)(170 * a)), djTitleText.c_str());
            dl->AddText(fontOverlay, sz, ImVec2(x, y), IM_COL32(255, 255, 255, (int)(255 * a)), djTitleText.c_str());
        }
        if (S.textOn && !S.text.empty()) {
        float size = S.textSize * H;
        ImVec2 ts = fontOverlay->CalcTextSizeA(size, FLT_MAX, 0, S.text.c_str());
        float y = S.textPos == 0 ? H * 0.06f : S.textPos == 1 ? (H - ts.y) / 2 : H * 0.94f - ts.y;
        float x = (W - ts.x) / 2;
        if (S.textScroll > 0) x = W - (float)std::fmod(t * S.textScroll * W * 0.25, (double)(W + ts.x));
        ImU32 shadow = IM_COL32(0, 0, 0, (int)(160 * modOut.textOpacity));
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(S.textR, S.textG, S.textB, modOut.textOpacity));
        float o = std::max(2.f, size * 0.04f);
        dl->AddText(fontOverlay, size, ImVec2(x + o, y + o), shadow, S.text.c_str());
        dl->AddText(fontOverlay, size, ImVec2(x, y), col, S.text.c_str());
        }
        ImGui::Render();
        rt.bind();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGui::SetCurrentContext(uiCtx);
    }
}

// ===================================================================== render pipeline
bool decodeAudioFile(const std::string& path, std::vector<float>& out, std::string& err) {
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 2, 48000);
    ma_decoder dec;
    if (ma_decoder_init_file(path.c_str(), &cfg, &dec) != MA_SUCCESS) { err = "unsupported or unreadable audio file"; return false; }
    out.clear();
    std::vector<float> b(8192 * 2);
    ma_uint64 got = 0;
    while (ma_decoder_read_pcm_frames(&dec, b.data(), 8192, &got) == MA_SUCCESS && got > 0) out.insert(out.end(), b.begin(), b.begin() + got * 2);
    ma_decoder_uninit(&dec);
    if (out.empty()) { err = "the file has no audio"; return false; }
    return true;
}

// Listen to the whole file first so the beat grid is right from the first frame.
double prescanTempo(const std::vector<float>& pcm) {
    Tempo t;
    double tt = 0;
    for (size_t i = 0; i + 960 <= pcm.size(); i += 960) {
        tt += 0.01;
        t.feed(&pcm[i], 480, tt);
        t.update(tt);
    }
    return t.confidence() > 1.5f ? t.bpm() : 0;
}

void finishRenderJob(bool cancelled) {
    recorder.stop();
    rj.lastOut = rj.out;
    rj.lastFrames = rj.frame;
    rj.lastSecs = rj.frame / (double)rj.fps;
    rj.lastWall = realNow() - rj.wallStart;
    rj.active = false;
    clockOffset = clockOverride - realNow();
    clockOverride = -1;
    S.autoOn = rj.sAutoOn, S.smartMix = rj.sSmart, S.autoFx = rj.sAutoFx, S.watchdog = rj.sWatchdog;
    tempo.restart(0);
    tempo.setSource((Tempo::Source)rj.sTempoSource);
    structure = MusicStructure();
    rj.pcm.clear();
    rj.pcm.shrink_to_fit();
    std::vector<float> drain(48000);
    while (audio.read(drain.data(), 24000) > 0) {}  // skip the live audio that piled up meanwhile
    if (cancelled) toast(true, "Render cancelled — kept the first %d:%02d in %s", (int)rj.lastSecs / 60, (int)rj.lastSecs % 60, rj.out.c_str());
    else toast(false, "Rendered %d:%02d of video in %.0f s -> %s", (int)rj.lastSecs / 60, (int)rj.lastSecs % 60, rj.lastWall, rj.out.c_str());
}

bool startRenderJob(std::vector<float> pcm, const std::string& src, const std::string& out, int w, int h, int fps, bool useAuto) {
    if (rj.active || recorder.active()) { toast(true, "Stop the current recording first"); return false; }
    std::error_code ec;
    fs::create_directories(fs::u8path(out).parent_path(), ec);
    // Some H.264 encoders refuse 4K or 60 fps: step down until one works and say what was used.
    std::string err;
    int tries[][3] = {{w, h, fps}, {w, h, 30}, {2560, 1440, fps}, {2560, 1440, 30}, {1920, 1080, fps}, {1920, 1080, 30}, {1280, 720, 30}};
    bool started = false;
    int usedW = w, usedH = h, usedFps = fps;
    for (auto& tr : tries) {
        if (tr[0] > w || tr[1] > h || tr[2] > fps) continue;
        if (recorder.start(out, tr[0], tr[1], tr[2], err, true)) {
            started = true, usedW = tr[0], usedH = tr[1], usedFps = tr[2];
            break;
        }
    }
    if (!started) { rj.lastErr = err; toast(true, "Render failed: %s", err.c_str()); return false; }
    if (usedW != w || usedFps != fps) toast(true, "This PC's video encoder can't do %dx%d @ %d fps - rendering %dx%d @ %d fps instead", w, h, fps, usedW, usedH, usedFps);
    fps = usedFps;
    rj = RenderJob{};
    rj.pcm = std::move(pcm);
    rj.src = src, rj.out = out, rj.w = recorder.width(), rj.h = recorder.height(), rj.fps = fps, rj.useAuto = useAuto;
    rj.total = (long long)((rj.pcm.size() / 2) / 48000.0 * fps) + fps;  // plus a second of tail
    rj.sAutoOn = S.autoOn, rj.sSmart = S.smartMix, rj.sAutoFx = S.autoFx, rj.sWatchdog = S.watchdog;
    rj.sTempoSource = (int)tempo.source();
    if (useAuto) S.autoOn = true, S.smartMix = true, S.autoFx = true;
    S.watchdog = false;
    rj.bpm = prescanTempo(rj.pcm);
    tempo.setSource(Tempo::Source::Auto);
    tempo.restart(rj.bpm);
    structure = MusicStructure();
    smart = Smart{};
    afx = AutoFx{};
    clockOverride = now();
    rj.wallStart = realNow();
    rj.active = true;
    return true;
}

// One step per main-loop pass. Returns true when this pass advances the video by one frame.
bool renderJobStep() {
    rj.feedNow = false;
    if (!rj.active) return false;
    if (rj.cancel) { finishRenderJob(true); return false; }
    // let presets finish compiling (no frame is ever skipped), but never wait more than 3 s on one
    static double waitSince = -1;
    if (anyLoading()) {
        if (waitSince < 0) waitSince = realNow();
        static double lastNote = 0;
        if (!selftestDir.empty() && realNow() - lastNote > 1) lastNote = realNow(), testNote("render waiting: A=" + std::to_string(decks[0].pendingIdx) + " B=" + std::to_string(decks[1].pendingIdx) + " spareLoadingB=" + std::to_string(decks[1].spare().loading()));
        if (realNow() - waitSince < 3) return false;
    }
    waitSince = -1;
    if (rj.frame >= rj.total) { finishRenderJob(false); return false; }
    rj.carry += 48000.0 / rj.fps;
    size_t n = (size_t)rj.carry;
    rj.carry -= n;
    rj.chunk.assign(n * 2, 0.f);
    for (size_t i = 0; i < n * 2 && rj.pos + i < rj.pcm.size(); i++) rj.chunk[i] = rj.pcm[rj.pos + i];
    rj.pos += n * 2;
    clockOverride += 1.0 / rj.fps;
    rj.feedNow = true;
    return true;
}

void captureRenderFrame() {
    rj.pixels.resize((size_t)rj.w * rj.h * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, finalRT.fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, rj.w, rj.h, GL_BGRA, GL_UNSIGNED_BYTE, rj.pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    double t0 = realNow();
    recorder.pushFrame(rj.pixels.data(), rj.chunk.data(), rj.chunk.size() / 2);
    if (!selftestDir.empty() && realNow() - t0 > 0.2) testNote("pushFrame blocked " + std::to_string(realNow() - t0) + " s at frame " + std::to_string(rj.frame));
    rj.frame++;
}

// ---- UI: pick a file, choose size, watch progress
bool renderDialogOpen = false;
std::string renderTestOut;
std::string renderPick;
std::vector<float> renderPickPcm;
int renderRes = 1, renderFps = 1;
bool renderUseAuto = true;
char renderOut[1024] = "";
const int kRenderRes[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
const char* kRenderResNames[] = {"720p", "1080p", "1440p", "4K"};

void openRenderDialog() {
    auto files = plat::pickFiles(hwndOf(win), L"Render an audio file to video",
                                 {{L"Audio", L"*.wav;*.flac;*.mp3;*.ogg"}, {L"All files", L"*.*"}}, false);
    if (files.empty()) return;
    std::string err;
    if (!decodeAudioFile(files[0], renderPickPcm, err)) { toast(true, "%s: %s", fs::u8path(files[0]).filename().u8string().c_str(), err.c_str()); return; }
    renderPick = files[0];
    std::string o = plat::videosDir() + "\\Amethyst\\" + fs::u8path(files[0]).stem().u8string() + " (Amethyst).mp4";
    snprintf(renderOut, sizeof renderOut, "%s", o.c_str());
    renderDialogOpen = true;
}

void drawRenderWindow() {
    if (!renderDialogOpen && !rj.active) return;
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);
    bool open = true;
    if (!ImGui::Begin("Render audio to video", rj.active ? nullptr : &open, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    if (!open) renderDialogOpen = false;
    if (rj.active) {
        float p = rj.total ? (float)rj.frame / rj.total : 0;
        double wall = realNow() - rj.wallStart, speed = wall > 0 ? (rj.frame / (double)rj.fps) / wall : 0;
        ImGui::TextUnformatted(fs::u8path(rj.src).filename().u8string().c_str());
        ImGui::ProgressBar(p, ImVec2(-1, 0));
        ImGui::Text("%lld / %lld frames   %.1fx real time   about %.0f s left   %s  %.0f BPM", rj.frame, rj.total, speed,
                    speed > 0 ? (rj.total - rj.frame) / (double)rj.fps / speed : 0.0, MusicStructure::name(structure.section()), tempo.bpm());
        if (ImGui::Button("Cancel")) rj.cancel = true;
    } else {
        ImGui::TextUnformatted(fs::u8path(renderPick).filename().u8string().c_str());
        double secs = renderPickPcm.size() / 2 / 48000.0;
        ImGui::TextDisabled("%d:%02d of audio", (int)secs / 60, (int)secs % 60);
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("Size", &renderRes, kRenderResNames, 4);
        ImGui::SameLine();
        static const char* fpsNames[] = {"30 fps", "60 fps"};
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("##fps", &renderFps, fpsNames, 2);
        ImGui::Checkbox("Let smart auto-mix + Auto FX run the show", &renderUseAuto);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off: renders exactly what you have set up now (presets, effects, auto settings).");
        ImGui::SetNextItemWidth(-90);
        ImGui::InputText("##out", renderOut, sizeof renderOut);
        ImGui::SameLine();
        if (ImGui::Button("Change...")) {
            auto o = plat::pickSaveFile(hwndOf(win), L"Save video as", {{L"MP4 video", L"*.mp4"}}, fs::u8path(renderOut).filename().u8string(), L"mp4");
            if (!o.empty()) snprintf(renderOut, sizeof renderOut, "%s", o.c_str());
        }
        ImGui::TextDisabled("Renders offline: every frame is kept, the music is added to the video.");
        if (S.videoOn) ImGui::TextColored(ImVec4(1, 0.72f, 0.3f, 1), "The video / camera / Spout layer plays in real time, so it is left out of file renders.");
        if (ImGui::Button("Render", ImVec2(120, 0))) {
            if (startRenderJob(std::move(renderPickPcm), renderPick, renderOut, kRenderRes[renderRes][0], kRenderRes[renderRes][1],
                               renderFps ? 60 : 30, renderUseAuto))
                renderDialogOpen = false;
        }
        if (!rj.lastOut.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Open last video")) plat::openPath(rj.lastOut);
        }
    }
    ImGui::End();
}

std::string userShaderDir() { return selftestDir.empty() ? plat::documentsDir() + "\\Amethyst\\Shaders" : selftestDir + "\\Shaders"; }
int shaderCompiles = 0;  // successful (re)compiles, for the hot-reload test
long long fileStamp(const std::string& p) {
    std::error_code ec;
    auto t = fs::last_write_time(fs::u8path(p), ec);
    return ec ? 0 : (long long)t.time_since_epoch().count();
}
void scanShaders() {
    for (auto& u : userShaders) if (u.prog) glDeleteProgram(u.prog);
    userShaders.clear();
    std::error_code ec;
    fs::create_directories(fs::u8path(userShaderDir()), ec);
    std::string readme = userShaderDir() + "\\README.txt";
    if (!fs::exists(fs::u8path(readme))) {
        std::ofstream r(fs::u8path(readme));
        r << "Put your own effects here as .glsl files. Write them like ShaderToy:\n\n"
             "  void mainImage(out vec4 fragColor, in vec2 fragCoord) { ... }\n\n"
             "Inputs: iChannel0 (the picture), iChannel1 (this effect's last output, for trails),\n"
             "iResolution, iTime, iFrame, iBeat, iBeatPhase (0..1 within the beat), iBass, iMid, iTreble, iLevel,\n"
             "iDrop (1 on a drop, fading out), iVocal. Amethyst blends the result with the Amount slider.\n"
             "Saving a file reloads it instantly. The bundled examples are in the 'shaders' folder next to Amethyst.exe.\n";
    }
    std::map<std::string, std::string> found;  // name -> path; your folder wins over the bundled one
    for (const std::string& dir : {plat::exeDir() + "\\shaders", userShaderDir()})
        for (fs::directory_iterator it(fs::u8path(dir), ec), end; !ec && it != end; it.increment(ec))
            if (it->path().extension() == ".glsl") found[it->path().stem().u8string()] = it->path().u8string();
    for (auto& [n, p] : found) userShaders.push_back({n, p});
}
bool ensureShader(UserShader& u) {
    long long st = fileStamp(u.path);
    if (u.tried && st == u.mtime) return u.prog != 0;
    u.tried = true;
    u.mtime = st;
    std::ifstream f(fs::u8path(u.path), std::ios::binary);
    std::string src((std::istreambuf_iterator<char>(f)), {});
    std::string err;
    unsigned p = gfx.compileUser(src, err);
    if (p) {
        if (u.prog) glDeleteProgram(u.prog);
        u.prog = p;
        u.err.clear();
        shaderCompiles++;
    } else {
        u.err = err;
        toast(true, "Shader %s: %s", u.name.c_str(), err.substr(0, 160).c_str());
    }
    return u.prog != 0;
}
int shaderIndex(const std::string& name) {
    for (size_t i = 0; i < userShaders.size(); i++) if (userShaders[i].name == name) return (int)i;
    return -1;
}
// which custom shader runs this frame, and how strongly
int activeShader(float& amount) {
    int m = S.userShader.empty() ? -1 : shaderIndex(S.userShader);
    if (m >= 0) { amount = S.userShaderAmount; return m; }
    if (autoFxOn() && S.autoFxShaders && afx.shader >= 0 && afx.shader < (int)userShaders.size()) {
        amount = S.autoFxAmount * (structure.section() == MusicStructure::Drop ? 0.9f : 0.6f);
        return afx.shader;
    }
    return -1;
}
RenderTarget userRT[2];
int userIdx = 0, userFrame = 0;
double lastShaderCheck = 0;

void renderFrame(double t, double dt) {
    int w, h;
    SDL_GL_GetDrawableSize(outputs.empty() ? win : outputs[0].win, &w, &h);
    float sc = kQuality[S.quality].scale;
    w = std::max(16, (int)(w * sc)) & ~1;
    h = std::max(16, (int)(h * sc)) & ~1;
    if (rj.active) w = rj.w, h = rj.h;  // the file renderer picks the size
    if (w != renderW || h != renderH) {
        renderW = w;
        renderH = h;
    }
    applyModulation(beatNow());
    autoFxApply(t, dt);
    // Deck B is only rendered when it can be seen.
    bool needB = modOut.xfade > 0.001f || S.mixMode != MixCrossfade || S.showMixer || xanim.active || smart.peeking;
    for (auto& o : outputs) needB |= o.source == 2;
    for (int d = 0; d < 2; d++) {
        if (d == 1 && !needB) {  // hidden: only warm up a preset that is loading, so the load can finish
            if (decks[1].spare().loading()) decks[1].spare().requestFrame(decks[1].time, w, h);
            continue;
        }
        Deck& dk = decks[d];
        if (!frozen) {
            dk.time += dt * S.speed;
            dk.visible().requestFrame(dk.time, w, h);
        }
        // the spare renders while it is loading (to warm up) or fading out
        if (dk.spare().loading() || dk.blending) dk.spare().requestFrame(dk.time, w, h);
        unsigned cur = dk.visible().texture();
        if (dk.blending) {
            float k = dk.blendManual >= 0 ? dk.blendManual : (float)std::clamp((t - dk.blendStart) / std::max(0.01, dk.blendDur), 0.0, 1.0);
            float e = dk.blendManual >= 0 ? k : k * k * k * (k * (k * 6 - 15) + 10);  // smootherstep: gentle in, gentle out
            dk.rt.ensure(w, h);
            if (dk.blendMode != MixCrossfade)  // both layers full, then the old one melts away (tease: new one glows in)
                gfx.mix(dk.rt, dk.spare().texture(), cur, dk.blendManual >= 0 ? k * 0.7f : 0.5f + 0.5f * e, dk.blendMode);
            else if (autoFxOn() && afx.transition > 0) gfx.transition(dk.rt, dk.spare().texture(), cur, e, afx.transition, (float)t);
            else gfx.mix(dk.rt, dk.spare().texture(), cur, e, MixCrossfade);
            dk.outTex = dk.rt.tex;
            if (dk.blendManual < 0 && k >= 1) dk.blending = false, dk.blendMode = MixCrossfade;
        } else {
            dk.outTex = cur;
        }
    }
    mixRT.ensure(w, h);
    for (auto& r : fxRT) r.ensure(w, h);
    finalRT.ensure(w, h);
    int trans = autoFxOn() && afx.transition >= 0 ? afx.transition : S.transition;
    if (xanim.active && trans > 0 && needB) {
        bool toB = xanim.to > xanim.from;
        float p = toB ? S.xfade : 1 - S.xfade;
        gfx.transition(mixRT, decks[toB ? 0 : 1].outTex, decks[toB ? 1 : 0].outTex, std::clamp(p, 0.f, 1.f), trans, (float)t);
    } else {
        int mm = S.mixMode;
        if (smashMode >= 0 && t < smashUntil) mm = smashMode;
        else if (smart.peeking && S.autoDecks) mm = MixScreen;
        gfx.mix(mixRT, decks[0].outTex, needB ? decks[1].outTex : decks[0].outTex, needB ? modOut.xfade : 0.f, mm);
    }
    fxRotation += (float)(S.fx.rotateSpeed * dt);
    int prev = fxIdx;
    fxIdx = 1 - fxIdx;
    gfx.fx(fxRT[fxIdx], mixRT.tex, fxRT[prev].tex, modOut.fx, fxRotation + modOut.rotOffset, (float)t);
    RenderTarget* post = &fxRT[fxIdx];
    {
        float amt = 0;
        int us = activeShader(amt);
        if (us >= 0 && (t - lastShaderCheck > 1 || !userShaders[us].tried)) {  // hot reload when the file is saved
            lastShaderCheck = t;
            ensureShader(userShaders[us]);
        }
        if (us >= 0 && userShaders[us].prog && amt > 0.01f) {
            int prevU = userIdx;
            userIdx = 1 - userIdx;
            for (auto& r : userRT) r.ensure(w, h);
            UserFxParams p;
            double b = beatNow();
            p.time = (float)t, p.beat = (float)b, p.beatPhase = (float)(b - std::floor(b));
            p.bass = bandBass, p.mid = bandMid, p.treble = bandTreb, p.level = bandLevel, p.amount = amt;
            double sinceDrop = b - afx.dropBeat;
            p.drop = sinceDrop >= 0 && sinceDrop < 8 ? (float)std::exp(-sinceDrop / 2) : 0.f;
            p.vocal = structure.vocal();
            p.frame = userFrame++;
            gfx.userFx(userRT[userIdx], userShaders[us].prog, fxRT[fxIdx].tex, userRT[prevU].tex, p);
            post = &userRT[userIdx];
        }
    }
    drawOverlays(*post, t);
    // strobe: on-beat or free-running white flashes (short duty cycle)
    float strobe = 0;
    if (modOut.fx.strobe) {
        double ph = S.fx.strobeOnBeat ? std::fmod(beatNow() * std::max(1.f, S.fx.strobeRate / 4.f), 1.0) : std::fmod(t * S.fx.strobeRate, 1.0);
        strobe = ph < 0.25 ? 1.f : 0.f;
    }
    gfx.finalize(finalRT, post->tex, std::max(blackAmt, afx.black), std::max({flashAmt, strobe, afx.white}));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (S.spout) {
        if (!spoutNamed) { spout.SetSenderName("Amethyst"); spoutNamed = true; }
        spout.SendTexture(finalRT.tex, GL_TEXTURE_2D, finalRT.w, finalRT.h, true, 0);
    }
    captureForRecording(dt);
    sendDmx(t, strobe);
}

