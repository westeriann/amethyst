// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== actions
void toggleAllUi();
void openEditor();
void dialogAddFiles();
void dialogAddFolder();
float logNorm(float v, float lo, float hi) { return (float)(std::log(v / lo) / std::log(hi / lo)); }
float logDenorm(float n, float lo, float hi) { return lo * std::pow(hi / lo, n); }

void registerActions() {
    auto trig = [](const char* id, const char* label, const char* group, std::function<void()> f) {
        Action a;
        a.id = id; a.label = label; a.group = group; a.kind = Action::Trigger; a.trigger = std::move(f);
        actions.add(a);
    };
    auto toggle = [](const char* id, const char* label, const char* group, bool* flag, std::function<void()> after = nullptr) {
        Action a;
        a.id = id; a.label = label; a.group = group; a.kind = Action::Toggle;
        a.trigger = [flag, after, label] { *flag = !*flag; if (after) after(); toast(false, "%s %s", label, *flag ? "on" : "off"); };
        a.get = [flag] { return *flag ? 1.f : 0.f; };
        actions.add(a);
    };
    auto cont = [](const char* id, const char* label, const char* group, std::function<void(float)> set, std::function<float()> get) {
        Action a;
        a.id = id; a.label = label; a.group = group; a.kind = Action::Continuous; a.set = std::move(set); a.get = std::move(get);
        actions.add(a);
    };

    trig("next", "Next", "Presets", [] { requestLaunch(focus, -1, 1, false, false, false); });
    trig("prev", "Previous", "Presets", [] { back(focus, false); });
    trig("random", "Random", "Presets", [] { requestLaunch(focus, -1, 1, false, true, false); });
    trig("next_hard", "Next (cut)", "Presets", [] { go(focus, 1, true); });
    trig("random_hard", "Random (cut)", "Presets", [] { go(focus, 1, true, true); });
    toggle("lock", "Lock", "Presets", &locked);
    trig("favorite", "Favorite", "Presets", [] { toggleFavorite(decks[focus].cur); });
    for (int i = 0; i < 10; i++) {
        static char ids[10][8], labels[10][8];
        snprintf(ids[i], 8, "cue%d", (i + 1) % 10);
        snprintf(labels[i], 8, "Cue %d", (i + 1) % 10);
        trig(ids[i], labels[i], "Cues", [i] { fireCue(i); });
    }

    trig("focus_toggle", "Focus A/B", "Mixer", [] { focus = 1 - focus; toast(false, "Focus: deck %s", decks[focus].name); });
    trig("a_next", "A next", "Mixer", [] { go(0, 1, false); });
    trig("a_random", "A random", "Mixer", [] { go(0, 1, false, true); });
    trig("b_next", "B next", "Mixer", [] { go(1, 1, false); });
    trig("b_random", "B random", "Mixer", [] { go(1, 1, false, true); });
    cont("crossfade", "Crossfader", "Mixer", [](float v) { S.xfade = v; xanim.active = false; }, [] { return S.xfade; });
    trig("xfade_auto", "Auto crossfade", "Mixer", [] { startXfade(S.xfade < 0.5f ? 1.f : 0.f, transitionSeconds()); });
    trig("mix_mode", "Mix mode", "Mixer", [] { S.mixMode = (S.mixMode + 1) % MixModeCount; toast(false, "Mix: %s", mixModeName(S.mixMode)); });
    toggle("auto_decks", "Auto-mix decks", "Mixer", &S.autoDecks);
    toggle("smart_mix", "Smart auto-mix (follow the song)", "Auto-pilot", &S.smartMix);
    toggle("auto_fx", "Auto FX", "Auto-pilot", &S.autoFx);
    toggle("auto_fx_flash", "Auto FX flashes", "Auto-pilot", &S.autoFxFlash);
    cont("auto_fx_amount", "Auto FX strength", "Auto-pilot", [](float v) { S.autoFxAmount = v; }, [] { return S.autoFxAmount; });

    toggle("auto", "Auto-pilot", "Auto-pilot", &S.autoOn, [] { decks[0].lastSwitch = decks[1].lastSwitch = now(); });
    toggle("shuffle", "Shuffle", "Auto-pilot", &S.shuffle);
    trig("quantize", "Quantize", "Auto-pilot", [] { S.quantize = (S.quantize + 1) % 4; toast(false, "Launch quantize: %s", kQuantizeNames[S.quantize]); });
    cont("interval", "Interval", "Auto-pilot", [](float v) { S.interval = (int)logDenorm(v, 4, 300); }, [] { return logNorm((float)S.interval, 4, 300); });
    trig("tap", "Tap tempo", "Tempo", [] { tempo.tap(now()); });
    trig("tempo_auto", "Auto BPM", "Tempo", [] { tempo.setSource(Tempo::Source::Auto); toast(false, "Tempo: automatic detection"); });
    trig("link", "Ableton Link", "Tempo", [] {
        S.link = !tempo.linkEnabled();
        tempo.enableLink(S.link);
        if (S.link) tempo.setSource(Tempo::Source::Link);
        toast(false, S.link ? "Ableton Link on" : "Ableton Link off");
    });

    toggle("blackout", "Blackout", "Performance", &blackoutOn);
    trig("flash", "Flash", "Performance", [] { flashAmt = 1; });
    trig("safe_now", "Safe preset now (panic)", "Performance", [] { panicSafe(); });
    trig("shader_next", "Next custom shader", "FX", [] {
        if (userShaders.empty()) { toast(true, "No custom shaders — add .glsl files to Documents\\Amethyst\\Shaders"); return; }
        int i = shaderIndex(S.userShader);
        S.userShader = i + 1 >= (int)userShaders.size() ? "" : userShaders[i + 1].name;
        toast(false, "Shader: %s", S.userShader.empty() ? "none" : S.userShader.c_str());
    });
    cont("shader_amount", "Shader amount", "FX", [](float v) { S.userShaderAmount = v; }, [] { return S.userShaderAmount; });
    toggle("watchdog", "Show safety watchdog", "Performance", &S.watchdog);
    { Action a; a.id = "freeze"; a.label = "Freeze (hold)"; a.group = "Performance"; a.kind = Action::Momentary;
      a.set = [](float v) { frozen = v > 0.5f; }; a.get = [] { return frozen ? 1.f : 0.f; }; actions.add(a); }
    cont("speed", "Speed", "Performance", [](float v) { S.speed = logDenorm(v, 0.1f, 4.f); }, [] { return logNorm(S.speed, 0.1f, 4.f); });
    trig("speed_reset", "Speed 1x", "Performance", [] { S.speed = 1; });
    cont("gain", "Sensitivity", "Performance", [](float v) { S.gain = logDenorm(v, 0.05f, 8.f); }, [] { return logNorm(std::clamp(S.gain, 0.05f, 8.f), 0.05f, 8.f); });
    cont("blend", "Blend time", "Performance", [](float v) { S.blend = v * 12; }, [] { return S.blend / 12; });

    toggle("fx_kaleido", "Kaleidoscope", "FX", &S.fx.kaleido);
    toggle("fx_mirror", "Mirror", "FX", &S.fx.mirror);
    toggle("fx_feedback", "Feedback", "FX", &S.fx.feedback);
    toggle("fx_rgb", "RGB split", "FX", &S.fx.rgbSplit);
    toggle("fx_pixel", "Pixelate", "FX", &S.fx.pixelate);
    toggle("fx_invert", "Invert", "FX", &S.fx.invert);
    toggle("fx_strobe", "Strobe", "FX", &S.fx.strobe);
    cont("fx_hue", "Hue", "FX", [](float v) { S.fx.hue = v; }, [] { return S.fx.hue; });
    cont("fx_zoom", "Zoom", "FX", [](float v) { S.fx.zoom = logDenorm(v, 0.4f, 4.f); }, [] { return logNorm(S.fx.zoom, 0.4f, 4.f); });
    cont("fx_rotate", "Rotate", "FX", [](float v) { S.fx.rotateSpeed = (v - 0.5f) * 4; }, [] { return S.fx.rotateSpeed / 4 + 0.5f; });
    cont("fx_swirl", "Swirl", "FX", [](float v) { S.fx.swirl = (v - 0.5f) * 4; }, [] { return S.fx.swirl / 4 + 0.5f; });
    cont("fx_feedback_amt", "Trails", "FX", [](float v) { S.fx.feedbackAmount = v * 0.98f; }, [] { return S.fx.feedbackAmount / 0.98f; });
    trig("fx_reset", "Reset FX", "FX", [] { S.fx = FxParams{}; toast(false, "Effects reset"); });

    toggle("logo", "Logo", "Overlays", &S.logoOn);
    toggle("text", "Text", "Overlays", &S.textOn);
    toggle("video", "Video layer", "Overlays", &S.videoOn, [] { if (S.videoOn && !video.active() && !S.videoPath.empty()) openVideo(S.videoPath); });
    cont("video_opacity", "Video opacity", "Overlays", [](float v) { S.videoOpacity = v; }, [] { return S.videoOpacity; });

    trig("record", "Record", "Output", [] { recorder.active() ? stopRecording() : startRecording(); });
    trig("spout", "Spout", "Output", [] { setSpout(!S.spout); });
    trig("output", "Output window", "Output", [] { if (!outputs.empty()) closeOutput(0); else openOutput(-2); });

    // ---- actions that used to be hard-wired keys; now freely re-assignable
    auto step = [&](const char* id, const char* label, const char* group, std::function<void()> f) {
        trig(id, label, group, std::move(f));
        actions.all().back().repeat = true;
    };
    auto local = [&](const char* id, const char* label, const char* group, std::function<void()> f) {
        trig(id, label, group, std::move(f));
        actions.all().back().remote = false;
    };
    trig("prev_hard", "Previous (cut)", "Presets", [] { back(focus, true); });
    step("blend_down", "Blend time -", "Presets", [] { S.blend = stepIn(kBlendSteps, S.blend, -1); toast(false, "Blend %.2gs", S.blend); });
    step("blend_up", "Blend time +", "Presets", [] { S.blend = stepIn(kBlendSteps, S.blend, 1); toast(false, "Blend %.2gs", S.blend); });
    for (int i = 0; i < 10; i++) {
        static char sid[10][16], slab[10][24], cid[10][16], clab[10][24];
        snprintf(sid[i], 16, "store_cue%d", (i + 1) % 10);
        snprintf(slab[i], 24, "Store cue %d", (i + 1) % 10);
        snprintf(cid[i], 16, "clear_cue%d", (i + 1) % 10);
        snprintf(clab[i], 24, "Clear cue %d", (i + 1) % 10);
        local(sid[i], slab[i], "Cues", [i] { storeCue(i); });
        local(cid[i], clab[i], "Cues", [i] { clearCue(i); });
    }
    step("xfade_left", "Crossfader to A", "Mixer", [] { S.xfade = std::max(0.f, S.xfade - 0.05f); xanim.active = false; });
    step("xfade_right", "Crossfader to B", "Mixer", [] { S.xfade = std::min(1.f, S.xfade + 0.05f); xanim.active = false; });
    step("interval_down", "Interval -", "Auto-pilot", [] { S.interval = stepIn(kIntervalSteps, S.interval, -1); toast(false, "Auto interval %ds", S.interval); });
    step("interval_up", "Interval +", "Auto-pilot", [] { S.interval = stepIn(kIntervalSteps, S.interval, 1); toast(false, "Auto interval %ds", S.interval); });
    trig("tempo_source", "Tempo source", "Tempo", [] {
        int s = ((int)tempo.source() + 1) % 3;
        if (s == 2 && !tempo.linkEnabled()) { tempo.enableLink(true); S.link = true; }
        tempo.setSource((Tempo::Source)s);
        toast(false, "Tempo source: %s", tempo.sourceName());
    });
    step("speed_down", "Speed -", "Performance", [] { S.speed = stepIn(kSpeedSteps, S.speed, -1); toast(false, "Speed %.2gx", S.speed); });
    step("speed_up", "Speed +", "Performance", [] { S.speed = stepIn(kSpeedSteps, S.speed, 1); toast(false, "Speed %.2gx", S.speed); });
    step("gain_up", "Sensitivity +", "Performance", [] { setGain(S.gain * 1.2f); });
    step("gain_down", "Sensitivity -", "Performance", [] { setGain(S.gain / 1.2f); });
    trig("quality", "Render quality", "Performance", [] {
        S.quality = (S.quality + 1) % 4;
        for (auto& d : decks) for (auto& p : d.players) p.setMesh(kQuality[S.quality].meshW, kQuality[S.quality].meshH);
        toast(false, "Quality: %s", kQuality[S.quality].label);
    });
    for (int r = 0; r <= 5; r++) {
        static char rid[6][8], rlab[6][24];
        snprintf(rid[r], 8, "rate%d", r);
        snprintf(rlab[r], 24, r ? "Rate %d stars" : "Clear rating", r);
        local(rid[r], rlab[r], "Library", [r] { setRating(decks[focus].cur, r); });
    }
    local("add_setlist", "Add to active setlist", "Library", [] { addToSetlist(decks[focus].cur, S.activeSetlist.empty() ? "Setlist 1" : S.activeSetlist); });
    local("add_files", "Add preset files", "Library", [] { dialogAddFiles(); });
    local("add_folder", "Add preset folder", "Library", [] { dialogAddFolder(); });
    local("rescan", "Rescan library", "Library", [] { rescan(); toast(false, "Library rescanned — %d presets", (int)lib.size()); });
    local("folder_filter", "Next folder filter", "Library", [] {
        sourceFilter = sourceFilter + 1 >= (int)sourceNames.size() ? -1 : sourceFilter + 1;
        rebuildView();
        toast(false, "Folder: %s (%d)", sourceFilter < 0 ? "All" : sourceNames[sourceFilter].c_str(), (int)view.size());
    });
    local("search", "Search presets", "Library", [] { S.showBrowser = true; S.uiHidden = false; focusSearch = true; });
    local("editor", "Preset editor", "Library", [] { if (editorOpen) editorOpen = false; else openEditor(); });

    auto panel = [&](const char* id, const char* label, bool* flag) {
        local(id, label, "View", [flag] { *flag = !*flag || S.uiHidden; S.uiHidden = false; });
    };
    local("ui_all", "Hide / show all interface", "View", [] { toggleAllUi(); });
    panel("panel_browser", "Preset browser", &S.showBrowser);
    panel("panel_hud", "Now-playing bar", &S.showHud);
    panel("panel_cues", "Cue strip", &S.showCues);
    panel("panel_mixer", "Mixer panel", &S.showMixer);
    panel("panel_fx", "Effects panel", &S.showFx);
    panel("panel_overlays", "Overlays panel", &S.showOverlays);
    panel("panel_outputs", "Outputs panel", &S.showOutputs);
    panel("panel_control", "Control panel", &S.showControl);
    panel("panel_setlists", "Setlists panel", &S.showSetlists);
    local("panel_menu", "Menu bar", "View", [] { S.showMenu = !S.showMenu; S.uiHidden = false; });
    local("previews", "Live previews", "View", [] { S.livePreviews = !S.livePreviews; toast(false, S.livePreviews ? "Live previews on" : "Live previews off"); });
    local("fullscreen", "Fullscreen", "View", [] { toggleFullscreen(keyWindow ? keyWindow : win); });
    local("help", "Shortcut help", "View", [] { helpOpen = !helpOpen; if (helpOpen) S.uiHidden = false; });
    local("keyboard", "Keyboard layout editor", "View", [] { keyboardOpen = !keyboardOpen; if (keyboardOpen) S.uiHidden = false; });
    local("audio_menu", "Audio input menu", "Audio", [] { refreshDevices(); audioMenuOpen = true; audioMenuSel = 0; S.uiHidden = false; });
    local("audio_file", "Play audio file", "Audio", [] { playAudioFileDialog(); });
    local("audio_pause", "Pause / resume file", "Audio", [] {
        if (audio.source() != AudioEngine::Source::File) toast(false, "No audio file playing");
        else { audio.togglePause(); toast(false, audio.paused() ? "Paused" : "Playing"); }
    });
    local("quit", "Quit", "View", [] { running = false; });
    for (int i = 0; i < kScenes; i++) {
        static char id[16][16], lb[16][24], sid[16][24], slb[16][24];
        snprintf(id[i], 16, "scene%d", i + 1);
        snprintf(lb[i], 24, "Scene %d", i + 1);
        snprintf(sid[i], 24, "store_scene%d", i + 1);
        snprintf(slb[i], 24, "Store scene %d", i + 1);
        trig(id[i], lb[i], "Scenes", [i] { recallScene(i); });
        local(sid[i], slb[i], "Scenes", [i] { storeScene(i); });
    }
    trig("timeline_play", "Play / stop show", "Scenes", [] { timelinePlay(!tlPlaying); });
    actions.all().back().get = [] { return tlPlaying ? 1.f : 0.f; };
    trig("timeline_next", "Next show step", "Scenes", [] {
        if (timeline.empty()) return;
        tlIndex = (tlIndex + 1) % (int)timeline.size();
        tlStepStart = std::floor(beatNow() / 4) * 4;
        recallScene(timeline[tlIndex].scene);
    });
    trig("transition", "Transition type", "Mixer", [] { S.transition = (S.transition + 1) % TransitionCount; toast(false, "Transition: %s", transitionName(S.transition)); });
    trig("dj_link", "DJ link", "Tempo", [] { setDjLink(!S.djOn); });
    actions.all().back().get = [] { return S.djOn ? 1.f : 0.f; };
    trig("dmx", "Art-Net lighting", "Output", [] { S.dmxOn = !S.dmxOn; toast(false, S.dmxOn ? "Art-Net on" : "Art-Net off"); });
    actions.all().back().get = [] { return S.dmxOn ? 1.f : 0.f; };
    panel("panel_scenes", "Scenes panel", &S.showScenes);
    panel("panel_mod", "Modulation panel", &S.showMod);
    local("similar", "More like the current preset", "Library", [] { if (decks[focus].cur >= 0) setPool("similar:" + lib[decks[focus].cur].path); });
}

// ===================================================================== key map
// keymap: (key, modifier bits: 1 Shift, 2 Ctrl, 4 Alt) -> action id. Fully user-editable.
void defaultKeymap() {
    keymap.clear();
    auto b = [](SDL_Keycode k, int m, const char* id) { keymap[{(int)k, m}] = id; };
    b(SDLK_SPACE, 0, "next"); b(SDLK_RIGHT, 0, "next"); b(SDLK_SPACE, 1, "next_hard"); b(SDLK_RIGHT, 1, "next_hard");
    b(SDLK_LEFT, 0, "prev"); b(SDLK_LEFT, 1, "prev_hard"); b(SDLK_r, 0, "random"); b(SDLK_r, 1, "random_hard");
    b(SDLK_LEFTBRACKET, 0, "blend_down"); b(SDLK_RIGHTBRACKET, 0, "blend_up"); b(SDLK_l, 0, "lock");
    for (int i = 0; i < 10; i++) {
        SDL_Keycode k = i == 9 ? SDLK_0 : SDLK_1 + i;
        std::string n = std::to_string((i + 1) % 10);
        keymap[{(int)k, 0}] = "cue" + n;
        keymap[{(int)k, 1}] = "store_cue" + n;
        keymap[{(int)k, 4}] = "clear_cue" + n;
        if (i < 5) keymap[{(int)k, 2}] = "rate" + std::to_string(i + 1);
    }
    b(SDLK_0, 2, "rate0");
    b(SDLK_TAB, 0, "focus_toggle"); b(SDLK_BACKSLASH, 0, "xfade_auto"); b(SDLK_SPACE, 2, "xfade_auto");
    b(SDLK_LEFT, 2, "xfade_left"); b(SDLK_RIGHT, 2, "xfade_right"); b(SDLK_j, 0, "mix_mode");
    b(SDLK_a, 0, "auto"); b(SDLK_COMMA, 0, "interval_down"); b(SDLK_PERIOD, 0, "interval_up"); b(SDLK_k, 0, "quantize"); b(SDLK_s, 0, "shuffle");
    b(SDLK_t, 0, "tap"); b(SDLK_t, 1, "tempo_source");
    b(SDLK_b, 0, "blackout"); b(SDLK_x, 0, "flash"); b(SDLK_z, 0, "freeze");
    b(SDLK_MINUS, 0, "speed_down"); b(SDLK_EQUALS, 0, "speed_up"); b(SDLK_BACKSPACE, 0, "speed_reset");
    b(SDLK_UP, 0, "gain_up"); b(SDLK_DOWN, 0, "gain_down"); b(SDLK_q, 0, "quality");
    b(SDLK_F2, 0, "fx_kaleido"); b(SDLK_F3, 0, "fx_mirror"); b(SDLK_F4, 0, "fx_feedback"); b(SDLK_F5, 0, "fx_rgb");
    b(SDLK_F6, 0, "fx_pixel"); b(SDLK_F7, 0, "fx_invert"); b(SDLK_F8, 0, "fx_strobe"); b(SDLK_F9, 0, "record"); b(SDLK_F12, 0, "spout");
    b(SDLK_f, 0, "fullscreen"); b(SDLK_F11, 0, "fullscreen"); b(SDLK_o, 0, "output"); b(SDLK_u, 0, "ui_all");
    b(SDLK_p, 0, "panel_browser"); b(SDLK_n, 0, "panel_hud"); b(SDLK_c, 0, "panel_cues"); b(SDLK_d, 0, "panel_mixer");
    b(SDLK_w, 0, "panel_fx"); b(SDLK_e, 0, "editor"); b(SDLK_F10, 0, "panel_menu"); b(SDLK_v, 0, "previews");
    b(SDLK_SLASH, 0, "search"); b(SDLK_f, 2, "search"); b(SDLK_SLASH, 1, "help"); b(SDLK_h, 0, "help"); b(SDLK_F1, 0, "help");
    b(SDLK_k, 2, "keyboard"); b(SDLK_g, 0, "folder_filter"); b(SDLK_i, 0, "audio_menu"); b(SDLK_m, 0, "audio_file"); b(SDLK_m, 1, "audio_pause");
    b(SDLK_o, 2, "add_files"); b(SDLK_o, 3, "add_folder"); b(SDLK_r, 2, "rescan"); b(SDLK_q, 2, "quit");
    b(SDLK_d, 2, "favorite"); b(SDLK_l, 2, "add_setlist");
    for (int i = 0; i < 8; i++) {
        keymap[{(int)(SDLK_F1 + i), 1}] = "scene" + std::to_string(i + 1);
        keymap[{(int)(SDLK_F1 + i), 3}] = "store_scene" + std::to_string(i + 1);
    }
    b(SDLK_t, 2, "timeline_play"); b(SDLK_j, 1, "transition"); b(SDLK_y, 0, "panel_mod"); b(SDLK_F1, 2, "panel_scenes"); b(SDLK_m, 2, "similar");
}

// Keypad keys act like their main-keyboard twins.
SDL_Keycode normalizeKey(SDL_Keycode k) {
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return SDLK_1 + (k - SDLK_KP_1);
    switch (k) {
        case SDLK_KP_0: return SDLK_0;
        case SDLK_KP_ENTER: return SDLK_RETURN;
        case SDLK_KP_PLUS: return SDLK_EQUALS;
        case SDLK_KP_MINUS: return SDLK_MINUS;
        case SDLK_KP_DIVIDE: return SDLK_SLASH;
        case SDLK_KP_PERIOD: return SDLK_PERIOD;
        default: return k;
    }
}

std::string keyName(SDL_Keycode k) {
    switch (k) {
        case SDLK_SPACE: return "Space";
        case SDLK_RETURN: return "Enter";
        case SDLK_BACKSPACE: return "Backspace";
        case SDLK_LEFT: return "Left";
        case SDLK_RIGHT: return "Right";
        case SDLK_UP: return "Up";
        case SDLK_DOWN: return "Down";
        case SDLK_PAGEUP: return "PgUp";
        case SDLK_PAGEDOWN: return "PgDn";
        case SDLK_DELETE: return "Del";
        case SDLK_INSERT: return "Ins";
        default: break;
    }
    const char* n = SDL_GetKeyName(k);
    return n && *n ? n : "?";
}

std::string chordName(int key, int mods) {
    std::string s;
    if (mods & 2) s += "Ctrl+";
    if (mods & 4) s += "Alt+";
    if (mods & 1) s += "Shift+";
    return s + keyName(key);
}

std::vector<std::string> chordsFor(const std::string& id) {
    std::vector<std::string> out;
    for (auto& [k, a] : keymap)
        if (a == id) out.push_back(chordName(k.first, k.second));
    return out;
}

// Short "[key]" hint for UI labels, reflecting the current key map.
std::string keyHint(const std::string& id) {
    auto c = chordsFor(id);
    return c.empty() ? "" : " [" + c.front() + "]";
}

void bindKey(int key, int mods, const std::string& id) {
    if (id.empty()) keymap.erase({key, mods});
    else keymap[{key, mods}] = id;
    saveSettings();
    Action* a = actions.find(id);
    if (!id.empty()) toast(false, "%s  ->  %s", chordName(key, mods).c_str(), a ? a->label.c_str() : id.c_str());
}

// ===================================================================== MIDI / OSC / web
void processMidi() {
    for (auto& m : midi.poll()) {
        int type = m.status & 0xF0, ch = m.status & 0x0F;
        bool isNote = type == 0x90 || type == 0x80, isCC = type == 0xB0;
        if (!isNote && !isCC) continue;
        std::string dev = midi.deviceName(m.device);
        if (!midi.learning.empty() && (isCC || (type == 0x90 && m.d2 > 0))) {
            int t = isCC ? 1 : 0;
            midi.bindings.erase(std::remove_if(midi.bindings.begin(), midi.bindings.end(),
                                               [&](const MidiBinding& b) { return b.device == dev && b.type == t && b.channel == ch && b.number == m.d1; }),
                                midi.bindings.end());
            midi.bindings.push_back({midi.learning, dev, t, ch, m.d1});
            Action* a = actions.find(midi.learning);
            toast(false, "MIDI: %s -> %s", Midi::describe(midi.bindings.back()).c_str(), a ? a->label.c_str() : "?");
            midi.learning.clear();
            saveSettings();
            continue;
        }
        for (auto& b : midi.bindings) {
            if (!b.device.empty() && b.device != dev) continue;
            if (b.channel != ch || b.number != m.d1 || b.type != (isCC ? 1 : 0)) continue;
            Action* a = actions.find(b.action);
            if (!a) continue;
            if (isNote) {
                bool down = type == 0x90 && m.d2 > 0;
                actions.invoke(b.action, down, a->kind == Action::Continuous && down, m.d2 / 127.f);
            } else if (a->kind == Action::Continuous) {
                actions.invoke(b.action, true, true, m.d2 / 127.f);
            } else {
                bool high = m.d2 >= 64;
                if (high != b.lastHigh) actions.invoke(b.action, high);
                b.lastHigh = high;
            }
        }
    }
}

void handleRemote(const std::string& rawAddr, bool hasValue, float value) {
    std::string a = rawAddr;
    if (a.rfind("/amethyst/", 0) == 0) a = a.substr(10);
    else if (!a.empty() && a[0] == '/') a = a.substr(1);
    for (auto& c : a)
        if (c == '/') c = '_';
    if (a.rfind("cue_", 0) == 0) a = "cue" + a.substr(4);
    Action* act = actions.find(a);
    if (!act) return;
    if (act->kind == Action::Continuous) actions.invoke(a, true, hasValue, value);
    else if (act->kind == Action::Momentary) actions.invoke(a, hasValue ? value > 0.5f : true);
    else if (!hasValue || value > 0.5f) actions.invoke(a, true);
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}

HttpResponse handleHttp(const std::string&, const std::string& path, const std::string& query) {
    HttpResponse r;
    if (path == "/" || path == "/index.html") {
        r.contentType = "text/html; charset=utf-8";
        r.body = kRemotePage;
    } else if (path == "/actions") {
        r.contentType = "application/json";
        r.body = "[";
        bool first = true;
        for (auto& a : actions.all()) {
            if (!a.remote) continue;
            const char* kind = a.kind == Action::Continuous ? "continuous" : a.kind == Action::Toggle ? "toggle" : a.kind == Action::Momentary ? "momentary" : "trigger";
            r.body += std::string(first ? "" : ",") + "{\"id\":\"" + a.id + "\",\"label\":\"" + jsonEscape(a.label) + "\",\"group\":\"" + a.group + "\",\"kind\":\"" + kind + "\"}";
            first = false;
        }
        r.body += "]";
    } else if (path == "/state") {
        r.contentType = "application/json";
        char b[256];
        double beat = beatNow();
        snprintf(b, sizeof b, "{\"x\":%.3f,\"bpm\":%.2f,\"src\":\"%s\",\"beat\":%d,", S.xfade, tempo.bpm(), tempo.sourceName(), ((int)std::floor(beat) % 4 + 4) % 4);
        r.body = b;
        r.body += "\"a\":\"" + jsonEscape(decks[0].cur >= 0 ? lib[decks[0].cur].name : "-") + "\",\"b\":\"" + jsonEscape(decks[1].cur >= 0 ? lib[decks[1].cur].name : "-") + "\",\"values\":{";
        bool first = true;
        for (auto& a : actions.all())
            if (a.get) {
                snprintf(b, sizeof b, "%s\"%s\":%.3f", first ? "" : ",", a.id.c_str(), a.get());
                r.body += b;
                first = false;
            }
        r.body += "}}";
    } else if (path.rfind("/a/", 0) == 0) {
        std::string id = path.substr(3);
        bool has = false;
        float v = 1;
        auto p = query.find("v=");
        if (p != std::string::npos) { has = true; v = (float)std::atof(query.c_str() + p + 2); }
        handleRemote("/" + id, has, v);
        r.body = "ok";
    } else {
        r.status = 404;
        r.body = "not found";
    }
    return r;
}

void applyNetSettings() {
    std::string err;
    if (S.oscOn && (!osc.running() || osc.port() != S.oscPort)) {
        if (!osc.start(S.oscPort, err)) { toast(true, "OSC: %s", err.c_str()); S.oscOn = false; }
    } else if (!S.oscOn && osc.running()) osc.stop();
    if (S.webOn && (!web.running() || web.port() != S.webPort)) {
        if (!web.start(S.webPort, err)) { toast(true, "Web remote: %s", err.c_str()); S.webOn = false; }
    } else if (!S.webOn && web.running()) web.stop();
}

// ===================================================================== files & drops
void addPresetFiles(const std::vector<std::string>& files) {
    std::string firstPath;
    for (auto& f : files) {
        if (std::find(S.files.begin(), S.files.end(), f) == S.files.end()) S.files.push_back(f);
        if (firstPath.empty()) firstPath = f;
    }
    rescan();
    saveSettings();
    toast(false, "Added %d preset%s", (int)files.size(), files.size() == 1 ? "" : "s");
    auto it = byPath.find(firstPath);
    if (it != byPath.end()) loadPreset(focus, it->second, false);
}
void dialogAddFiles() {
    auto files = plat::pickFiles(hwndOf(win), L"Add MilkDrop presets", {{L"MilkDrop presets", L"*.milk;*.prjm"}}, true);
    if (!files.empty()) addPresetFiles(files);
}
void dialogAddFolder() {
    auto d = plat::pickFolder(hwndOf(win), L"Add a folder of MilkDrop presets");
    if (!d.empty()) addFolder(d);
}

std::vector<std::string> pendingDrops;
void processDrops() {
    if (pendingDrops.empty()) return;
    std::vector<std::string> presets;
    for (auto& d : pendingDrops) {
        fs::path p = fs::u8path(d);
        std::error_code ec;
        if (fs::is_directory(p, ec)) addFolder(d);
        else if (isPresetFile(p)) presets.push_back(d);
        else if (isAudioFile(p)) startAudioFrom("file", d, false);
        else if (isVideoFile(p)) openVideo(d);
        else if (isImageFile(p)) { S.logoPath = d; S.logoOn = true; toast(false, "Logo overlay: %s", p.filename().u8string().c_str()); saveSettings(); }
        else toast(true, "Can't use %s", p.filename().u8string().c_str());
    }
    if (!presets.empty()) addPresetFiles(presets);
    pendingDrops.clear();
}

// ===================================================================== preset editor
void openEditor() {
    int cur = decks[focus].cur;
    editorOpen = true;
    S.uiHidden = false;
    if (cur < 0) return;
    std::ifstream f(fs::u8path(lib[cur].path), std::ios::binary);
    editorText.assign((std::istreambuf_iterator<char>(f)), {});
    editorPath = lib[cur].path;
    editorDirty = false;
    editorApplyAt = 0;
    visualEditor.open(editorText);
    editorStatus = "Editing deck " + std::string(decks[focus].name) + ": " + lib[cur].name;
}
void applyEditor() {
    Deck& d = decks[focus];
    d.blending = false;
    d.pendingIdx = d.cur;
    d.pendingToken = ++loadTokens;
    d.pendingEditor = true;
    d.spare().loadData(editorText, d.pendingToken);
    editorStatus = "Compiling...";
}
void saveEditorAs() {
    std::string def = editorPath.empty() ? "My preset.milk" : fs::u8path(editorPath).stem().u8string() + " (edit).milk";
    auto out = plat::pickSaveFile(hwndOf(win), L"Save preset as", {{L"MilkDrop preset", L"*.milk"}}, def, L"milk");
    if (out.empty()) return;
    std::ofstream f(fs::u8path(out), std::ios::binary | std::ios::trunc);
    f << editorText;
    f.close();
    editorPath = out;
    editorDirty = false;
    editorStatus = "Saved " + out;
    addPresetFiles({out});
}

// ===================================================================== keyboard
void toggleAllUi() {
    S.uiHidden = !S.uiHidden;
    if (!S.uiHidden) toast(false, "Interface shown");
}

int digitSlot(SDL_Keycode k, SDL_Scancode sc) {
    if (k >= SDLK_1 && k <= SDLK_9) return k - SDLK_1;
    if (k == SDLK_0) return 9;
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return k - SDLK_KP_1;
    if (k == SDLK_KP_0) return 9;
    bool printable = k >= 32 && k < 127, letter = k >= 'a' && k <= 'z';
    if (printable && !letter && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) return sc - SDL_SCANCODE_1;
    return -1;
}

void audioMenuActivate(int i);

bool handleKey(const SDL_KeyboardEvent& e, bool textInputActive) {
    const SDL_Keycode k = e.keysym.sym;
    const Uint16 mod = e.keysym.mod;
    const bool sh = mod & KMOD_SHIFT, ctrl = mod & KMOD_CTRL, alt = mod & KMOD_ALT, rep = e.repeat;
    SDL_Window* from = SDL_GetWindowFromID(e.windowID);

    if (textInputActive) {
        if (k == SDLK_ESCAPE) { defocusSearch = true; return true; }  // leave the field, keep its text
        if (!searchHasFocus) {  // editor or another text field
            if (editorOpen && ctrl && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) { applyEditor(); return true; }
            if (editorOpen && ctrl && k == SDLK_s) { saveEditorAs(); return true; }
            return false;
        }
        switch (k) {
            case SDLK_DOWN: sel = std::min((int)view.size() - 1, sel + 1); scrollToSel = true; return true;
            case SDLK_UP: sel = std::max(0, sel - 1); scrollToSel = true; return true;
            case SDLK_PAGEDOWN: sel = std::min((int)view.size() - 1, sel + 10); scrollToSel = true; return true;
            case SDLK_PAGEUP: sel = std::max(0, sel - 10); scrollToSel = true; return true;
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (sel >= 0 && sel < (int)view.size()) loadPreset(focus, view[sel], sh);
                return true;
            default: return false;
        }
    }
    if (k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL || k == SDLK_RCTRL || k == SDLK_LALT || k == SDLK_RALT || k == SDLK_LGUI || k == SDLK_RGUI)
        return false;
    if (audioMenuOpen) {
        int count = (int)audioDevices.size() + 2;
        if (k == SDLK_DOWN) audioMenuSel = (audioMenuSel + 1) % count;
        else if (k == SDLK_UP) audioMenuSel = (audioMenuSel + count - 1) % count;
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) audioMenuActivate(audioMenuSel);
        else if (k == SDLK_ESCAPE || k == SDLK_i) audioMenuOpen = false;
        return true;
    }
    if (!midi.learning.empty() && k == SDLK_ESCAPE) { midi.learning.clear(); toast(false, "MIDI learn cancelled"); return true; }
    if (k == SDLK_ESCAPE) {
        if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return false;
        if (!captureAction.empty()) { captureAction.clear(); toast(false, "Key capture cancelled"); }
        else if (helpOpen) helpOpen = false;
        else if (keyboardOpen) keyboardOpen = false;
        else if (editorOpen) editorOpen = false;
        else if (!query.empty()) { query.clear(); searchBuf[0] = 0; rebuildView(); toast(false, "Search cleared"); }
        else if (from && (SDL_GetWindowFlags(from) & SDL_WINDOW_FULLSCREEN_DESKTOP)) toggleFullscreen(from);
        return true;
    }
    keyWindow = from;
    const int mods = (sh ? 1 : 0) | (ctrl ? 2 : 0) | (alt ? 4 : 0);
    SDL_Keycode key = normalizeKey(k);
    if (!captureAction.empty()) {  // keyboard editor: "press the key for this action"
        bindKey((int)key, mods, captureAction);
        captureAction.clear();
        return true;
    }
    auto it = keymap.find({(int)key, mods});
    if (it == keymap.end()) {
        // Layouts where the number row types symbols (AZERTY...): fall back to the key's position.
        SDL_Scancode sc = e.keysym.scancode;
        bool printable = k >= 32 && k < 127, letter = k >= 'a' && k <= 'z';
        if (printable && !letter && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) {
            int i = sc - SDL_SCANCODE_1;
            key = i == 9 ? SDLK_0 : SDLK_1 + i;
            it = keymap.find({(int)key, mods});
        }
    }
    if (it == keymap.end()) return false;
    Action* a = actions.find(it->second);
    if (!a) return false;
    if (rep && !a->repeat) return true;
    if (a->kind == Action::Momentary) heldMomentary[a->id] = key;
    actions.invoke(a->id, true);
    return true;
}

void handleKeyUp(const SDL_KeyboardEvent& e) {
    SDL_Keycode key = normalizeKey(e.keysym.sym);
    for (auto it = heldMomentary.begin(); it != heldMomentary.end();) {
        if (it->second == key) { actions.invoke(it->first, false); it = heldMomentary.erase(it); }
        else ++it;
    }
}

void releaseHeldKeys() {
    for (auto& [id, k] : heldMomentary) actions.invoke(id, false);
    heldMomentary.clear();
}


void audioMenuActivate(int i) {
    audioMenuOpen = false;
    if (i < (int)audioDevices.size()) {
        auto& d = audioDevices[i];
        startAudioFrom(d.loopback ? "loopback" : "input", d.name, false);
    } else if (i == (int)audioDevices.size()) playAudioFileDialog();
    else startAudioFrom("none", "", false);
}

