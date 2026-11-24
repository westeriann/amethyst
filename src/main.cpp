// Amethyst (native) — keyboard-, MIDI- and remote-driven MilkDrop VJ software built on libprojectM.
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <projectM-4/projectM.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

#include "SpoutReceiver.h"
#include "SpoutSender.h"

#include "actions.h"
#include "image.h"
#include "audio.h"
#include "gfx.h"
#include "media.h"
#include "midi.h"
#include "net.h"
#include "platform.h"
#include "player.h"
#include "preset_editor.h"
#include <miniaudio/miniaudio.h>
#include "structure.h"
#include "remote_page.h"
#include "stagelinq.h"
#include "tempo.h"
#include "thumbs.h"

#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <tuple>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
// The app is one translation unit split by area; the parts are included in this order because later
// parts use what earlier ones define.
#include "app/01_settings_state.inc"   // settings, global state, persistence
#include "app/02_library_decks.inc"    // preset library, decks, loading, navigation
#include "app/03_audio_outputs.inc"    // audio input, output windows, Spout, recording
#include "app/04_live_features.inc"    // modulation, scenes, timeline, Spout in, Art-Net, DJ link, MIDI LEDs
#include "app/05_smart_library.inc"    // preset analysis, colour/energy pools, similarity
#include "app/06_auto_fx.inc"          // Auto FX
#include "app/07_auto_mix.inc"         // smart auto-mix (song structure -> switches)
#include "app/08_render.inc"           // overlays, file renderer, custom shaders, render pipeline
#include "app/09_actions_input.inc"    // actions, key map, MIDI/OSC/web, drops, preset editor, keyboard
#include "app/10_ui.inc"               // interface: helpers, panels, keyboard editor, help
#include "app/11_safety.inc"           // show safety: watchdog, safe preset, crash recovery
#include "app/12_tests.inc"            // self-test, perf test, render test

}  // namespace

// ===================================================================== main
// One-time move from the old "Vortex VJ" name: settings in %APPDATA%, presets/textures in Documents.
void migrateLegacy() {
    std::error_code ec;
    fs::path newApp = fs::u8path(plat::appDataDir());
    fs::path oldApp = newApp.parent_path() / "VortexVJ";
    fs::path docs = fs::u8path(plat::documentsDir());
    fs::path oldDocs = docs / "Vortex VJ", newDocs = docs / "Amethyst";
    if (fs::is_directory(oldDocs, ec) && !fs::exists(newDocs, ec)) fs::rename(oldDocs, newDocs, ec);
    if (!fs::is_directory(oldApp, ec) || fs::exists(newApp / "settings.ini", ec)) return;
    for (auto& e : fs::directory_iterator(oldApp, ec)) {
        std::error_code e2;
        if (!e.is_regular_file(e2)) continue;
        std::ifstream in(e.path(), std::ios::binary);
        std::string txt((std::istreambuf_iterator<char>(in)), {});
        auto replaceAll = [&](const std::string& from, const std::string& to) {
            for (size_t p = 0; (p = txt.find(from, p)) != std::string::npos; p += to.size()) txt.replace(p, from.size(), to);
        };
        replaceAll(oldDocs.u8string(), newDocs.u8string());
        replaceAll("\Vortex - ", "\Amethyst - ");
        replaceAll("Vortex VJ", "Amethyst");
        std::ofstream out(newApp / e.path().filename(), std::ios::binary | std::ios::trunc);
        out << txt;
    }
}

double startTime = 0;
int main(int argc, char** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    startTime = now();
    std::string extraFolder;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--selftest" && i + 1 < argc) selftestDir = argv[++i];
        else if (a == "--perftest" && i + 1 < argc) { selftestDir = argv[++i]; perfMode = true; }
        else if (a == "--add-folder" && i + 1 < argc) extraFolder = argv[++i];
        else if (a == "--recovered") recoveredStart = true;
        else if (a == "--rendertest" && i + 1 < argc) { selftestDir = argv[++i]; renderTestOnly = true; }
        else if (a == "--crashtest" && i + 1 < argc) crashTestDir = argv[++i];
    }
    if (selftestDir.empty()) migrateLegacy();
    std::string base = !crashTestDir.empty() ? crashTestDir : selftestDir.empty() ? plat::appDataDir() : selftestDir;
    gDataDir = base;
    if (!crashTestDir.empty()) fs::create_directories(fs::u8path(crashTestDir));
    if (!selftestDir.empty()) {
        fs::create_directories(fs::u8path(selftestDir));
        for (const char* f : {"settings.ini", "library.ini", "layout.ini", "scenes.ini", "analysis.ini"}) { std::error_code ec; fs::remove(fs::u8path(selftestDir + "\\" + f), ec); }
        testLog.open(fs::u8path(selftestDir + "\\selftest.log"), std::ios::trunc);
        S.docsFolderAdded = true;  // sandbox: only the folders we pass in
    }
    gSettingsPath = base + "\\settings.ini";
    gLibraryPath = base + "\\library.ini";
    gImguiIni = base + "\\layout.ini";
    gScenesPath = base + "\\scenes.ini";
    gAnalysisPath = base + "\\analysis.ini";
    loadSettings();
    loadLibrary();
    loadScenes();
    loadLooks();
    if (!extraFolder.empty()) S.folders.push_back(extraFolder);
    if (selftestDir.empty()) {  // the bundled presets folder next to the exe is always in the library
        std::string bundled = (fs::u8path(plat::exeDir()) / "presets").u8string();
        std::error_code ec;
        if (fs::is_directory(fs::u8path(bundled), ec) && std::find(S.folders.begin(), S.folders.end(), bundled) == S.folders.end())
            S.folders.insert(S.folders.begin(), bundled);
    }
    if (!S.docsFolderAdded) {
        std::string docs = (fs::u8path(plat::documentsDir()) / "Amethyst" / "Presets").u8string();
        std::error_code ec;
        if (fs::is_directory(fs::u8path(docs), ec) && std::find(S.folders.begin(), S.folders.end(), docs) == S.folders.end()) S.folders.push_back(docs);
        S.docsFolderAdded = true;
    }

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        MessageBoxA(nullptr, SDL_GetError(), "Amethyst", MB_ICONERROR);
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("Amethyst", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440, 900,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);
    ctx = win ? SDL_GL_CreateContext(win) : nullptr;
    if (!ctx) {
        MessageBoxA(nullptr, "OpenGL 3.3 is required. Please update your graphics driver.", "Amethyst", MB_ICONERROR);
        return 1;
    }
    glewExperimental = GL_TRUE;
    glewInit();
    glGetError();
    thumbWin = SDL_CreateWindow("", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    thumbCtx = SDL_GL_CreateContext(thumbWin);
    SDL_GL_MakeCurrent(win, ctx);

    // Four preset players (two per deck), each with a hidden window + shared GL context and its own thread.
    for (auto& d : decks)
        for (auto& p : d.players) {
            SDL_Window* pw = SDL_CreateWindow("", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
            SDL_GL_MakeCurrent(win, ctx);
            SDL_GLContext pc = pw ? SDL_GL_CreateContext(pw) : nullptr;
            SDL_GL_MakeCurrent(win, ctx);
            if (!pc) {
                MessageBoxA(nullptr, "Could not create the rendering threads (OpenGL context sharing failed).", "Amethyst", MB_ICONERROR);
                return 1;
            }
            playerWindows.push_back(pw);
            playerContexts.push_back(pc);
            p.setMesh(kQuality[S.quality].meshW, kQuality[S.quality].meshH);
            p.start(pw, pc);
        }
    gfx.init();
    if (thumbCtx) {
        thumbs.setFps(S.previewFps);
        thumbs.start(thumbWin, thumbCtx, 192, 108);
    }

    // ImGui: UI context + an overlay context (shares the font atlas) for text overlays
    IMGUI_CHECKVERSION();
    uiCtx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = gImguiIni.c_str();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    float ddpi = 96;
    SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(win), &ddpi, nullptr, nullptr);
    float scale = std::clamp(ddpi / 96.f, 1.f, 3.f);
    {
        static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0100, 0x024F, 0x2000, 0x206F, 0x2190, 0x21FF, 0};
        static const ImWchar symRanges[] = {0x2600, 0x26FF, 0};
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        bool segoe = fs::exists("C:\\Windows\\Fonts\\segoeui.ttf");
        if (segoe) io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17 * scale, &cfg, ranges);
        else io.Fonts->AddFontDefault();
        if (fs::exists("C:\\Windows\\Fonts\\seguisym.ttf")) {
            ImFontConfig m;
            m.MergeMode = true;
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 15 * scale, &m, symRanges);
        }
        if (fs::exists("C:\\Windows\\Fonts\\seguisb.ttf")) fontBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 25 * scale, &cfg, ranges);
        if (fs::exists("C:\\Windows\\Fonts\\segoeuib.ttf")) fontOverlay = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 96, &cfg, ranges);
        if (!fontBig) fontBig = io.Fonts->Fonts[0];
        if (!fontOverlay) fontOverlay = io.Fonts->Fonts[0];
    }
    setupStyle(scale);
    ImGui_ImplSDL2_InitForOpenGL(win, ctx);
    ImGui_ImplOpenGL3_Init("#version 330");
    overlayCtx = ImGui::CreateContext(io.Fonts);
    ImGui::SetCurrentContext(overlayCtx);
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::SetCurrentContext(uiCtx);

    registerActions();
    defaultKeymap();
    if (!savedKeys.empty()) {  // user's map replaces the defaults; unknown actions are dropped
        keymap.clear();
        for (auto& [k, m, id] : savedKeys)
            if (m >= 0 && actions.find(id)) keymap[{k, m}] = id;
    }
    midi.open();
    for (auto& [dev, st] : midi.feedbackStyle)
        if (st >= 2) midi.sendSysex(dev, launchpadSysex(st - 1));
    tempo.setBpm(S.tapBpm, now());
    if (S.link) tempo.enableLink(true);
    tempo.setSource((Tempo::Source)std::clamp(S.tempoSource, 0, 2));
    applyNetSettings();
    if (S.videoOn) openLayerSource();
    if (S.djOn) djLink.start();

    audio.init();
    refreshDevices();
    if (S.audioKind == "file" && !fs::exists(fs::u8path(S.audioDevice))) S.audioKind = "loopback", S.audioDevice.clear();
    startAudioFrom(S.audioKind, S.audioDevice, true);

    rescan();
    scanShaders();
    if (selftestDir.empty()) {
        sessionCheckAtStart();
        if (lastRunCrashed && !crashedPreset.empty()) {
            auto cit = byPath.find(crashedPreset);
            if (cit != byPath.end()) markBroken(cit->second, "crashed Amethyst");
            for (std::string* last : {&S.lastA, &S.lastB})
                if (*last == crashedPreset) last->clear();
        }
        if (recoveredStart) toast(true, "Amethyst restarted itself after a problem and is back on air");
        else if (lastRunCrashed) toast(true, "Amethyst did not close properly last time%s", crashedPreset.empty() ? "" : " — the preset that was loading is now marked broken");
        sessionMark("");
        plat::installCrashRestart(!recoveredStart);  // after an auto-restart, arm again only once it has run a while
    }
    for (int d = 0; d < 2; d++) {
        auto it = byPath.find(d ? S.lastB : S.lastA);
        int first = it != byPath.end() ? it->second : (view.empty() ? -1 : (d ? pickRandom(-1) : view[0]));
        if (first >= 0 && !loadPreset(d, first, true)) go(d, 1, true);
    }
    for (auto& so : savedOutputs) {
        if (so.size() < 10) continue;
        int disp = std::atoi(so[0].c_str());
        if (disp >= SDL_GetNumVideoDisplays() || disp == SDL_GetWindowDisplayIndex(win)) continue;
        float c[8];
        for (int k = 0; k < 8; k++) c[k] = (float)std::atof(so[2 + k].c_str());
        openOutput(disp, std::atoi(so[1].c_str()), c);
    }
    if (lib.empty()) toast(false, "Drop .milk presets or a folder onto the window to begin");
    else toast(false, "%d presets  -  %s  -  press H for shortcuts", (int)lib.size(), audio.label().c_str());

    double last = now();
    if (!selftestDir.empty()) { if (perfMode) buildPerfTest(); else buildSelftest(); testStart = now(); }
    SDL_Event e;
    while (running) {
        while (!selftestDir.empty() && testNext < testSteps.size() && now() - testStart >= testSteps[testNext].at) testSteps[testNext++].act();
        const bool textActive = ImGui::GetIO().WantTextInput;
        while (SDL_PollEvent(&e)) {
            bool consumed = false;
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_WINDOWEVENT: {
                    int oi = outputIndexForWindow(e.window.windowID);
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                        if (oi >= 0) { closeOutput(oi); consumed = true; }
                        else if (e.window.windowID == SDL_GetWindowID(win)) running = false;
                    }
                    if (oi >= 0) {
                        if (e.window.event == SDL_WINDOWEVENT_ENTER) SDL_ShowCursor(SDL_DISABLE);
                        if (e.window.event == SDL_WINDOWEVENT_LEAVE) SDL_ShowCursor(SDL_ENABLE);
                        consumed = true;
                    }
                    break;
                }
                case SDL_KEYDOWN: consumed = handleKey(e.key, textActive); break;
                case SDL_KEYUP: handleKeyUp(e.key); break;
                case SDL_MOUSEMOTION:
                    lastMouseMove = now();
                    if (outputIndexForWindow(e.motion.windowID) >= 0) consumed = true;
                    break;
                case SDL_DROPFILE:
                    pendingDrops.push_back(e.drop.file);
                    SDL_free(e.drop.file);
                    consumed = true;
                    break;
                default: break;
            }
            if (!consumed) ImGui_ImplSDL2_ProcessEvent(&e);
        }
        bool anyFocus = SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS;
        for (auto& o : outputs) anyFocus |= (SDL_GetWindowFlags(o.win) & SDL_WINDOW_INPUT_FOCUS) != 0;
        if (!anyFocus && selftestDir.empty()) releaseHeldKeys();
        processDrops();
        processMidi();
        osc.poll(handleRemote);
        web.poll(handleHttp);

        bool rjAdvance = renderJobStep();
        const double t = now();
        const double dt = std::clamp(t - last, 0.0, 0.25);
        if (perfMode) perfFrames.push_back((t - last) * 1000.0);
        last = t;

        pumpAudio();
        tempo.update(t);
        updateDjLink(t);
        updateMorph();
        updateTimeline();
        intakeAnalysis(t);
        {   // background analysis yields whenever the show needs the GPU
            static double slowSince = 0;
            float fps = ImGui::GetIO().Framerate;
            if (fps < 50) slowSince = t;
            // Compiling presets for analysis briefly stalls the GPU driver, so never do it during a show:
            // outputs open, fullscreen, interface hidden, Spout or recording running.
            bool performing = !outputs.empty() || (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) || S.uiHidden || S.spout ||
                              recorder.active();
            thumbs.analysisEnabled = S.analysisOn && ((!performing && t - slowSince > 3.0) || analyseNow);
        }
        updateLeds(t);
        // crossfade animation
        if (xanim.active) {
            float k = (float)std::clamp((t - xanim.start) / xanim.dur, 0.0, 1.0);
            k = k * k * k * (k * (k * 6 - 15) + 10);  // smootherstep
            S.xfade = xanim.from + (xanim.to - xanim.from) * k;
            if (k >= 1) xanim.active = false;
        }
        // quantized launch
        if (launch.armed && beatNow() >= launch.dueBeat - 0.02) {
            launch.armed = false;
            if (launch.idx == -2) back(launch.deck, launch.hard);
            else if (launch.random || launch.idx < 0) { AutoPickScope scope(launch.fromAuto); go(launch.deck, 1, launch.hard, true); }
            else if (!loadPreset(launch.deck, launch.idx, launch.hard)) go(launch.deck, 1, true);
        }
        // auto-pilot
        smartTick(t);
        if (S.autoOn && !S.smartMix && !locked && !launch.armed && !xanim.active) {
            int on = S.xfade < 0.5f ? 0 : 1;
            if (t - decks[on].lastSwitch >= S.interval) {
                decks[on].lastSwitch = t;
                if (S.quantize > 0 && !S.autoDecks) requestLaunch(on, -1, 1, false, S.shuffle, true);
                else autoAdvance();
            }
        }
        for (auto& f : thumbs.takeFailures()) {
            auto it = byPath.find(f);
            if (it != byPath.end()) markBroken(it->second, "preview failed to load");
        }
        if (upcoming < 0 || upcoming == decks[0].cur || upcoming == decks[1].cur) upcoming = pickRandom(decks[focus].cur);

        float target = blackoutOn ? 1.f : 0.f;
        blackAmt += std::clamp(target - blackAmt, (float)-dt / 0.8f, (float)dt / 0.8f);
        flashAmt = std::max(0.f, flashAmt - (float)dt / 0.35f);

        SDL_GL_MakeCurrent(win, ctx);
        commitLoads();
        renderFrame(t, dt);
        if (rjAdvance) captureRenderFrame();
        if (!rj.active) watchdogTick(t, dt);
        if (!crashTestDir.empty() && t - startTime > 4) {
            if (recoveredStart) {  // we are the restarted copy: report and leave
                std::ofstream(fs::u8path(crashTestDir + "\\recovered.txt")) << "restarted after crash, presets=" << lib.size() << "\n";
                running = false;
            } else {
                std::ofstream(fs::u8path(crashTestDir + "\\crashing.txt")) << "about to crash\n";
                plat::installCrashRestart(true);
                volatile int* bad = nullptr;
                *bad = 42;  // the crash
            }
        }
        {
            static double lastSave = t;
            if (selftestDir.empty() && t - lastSave > 60) { lastSave = t; saveSettings(); }  // a crash loses little
            static bool armed = false;
            if (!armed && t - lastSave >= 0 && recoveredStart && selftestDir.empty() && t > 30 + startTime) { armed = true; plat::installCrashRestart(true); }
        }

        // control window
        int cw, ch;
        SDL_GL_GetDrawableSize(win, &cw, &ch);
        SDL_GL_SetSwapInterval(outputs.empty() && !rj.active ? 1 : 0);  // rendering a file runs as fast as it can
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, cw, ch);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        {
            float a = renderW / (float)std::max(1, renderH), ca = cw / (float)std::max(1, ch);
            int w = cw, h = ch, x = 0, y = 0;
            if (a > ca) { h = (int)(cw / a); y = (ch - h) / 2; } else { w = (int)(ch * a); x = (cw - w) / 2; }
            gfx.present(finalRT.tex, x, y, w, h, nullptr, false);
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        drawUi();
        if (S.uiHidden && t - lastMouseMove > 2.0) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        ImGui::Render();
        glViewport(0, 0, cw, ch);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!pendingShot.empty()) { captureBackbuffer(pendingShot, cw, ch); pendingShot.clear(); }
        SDL_GL_SwapWindow(win);

        for (size_t i = 0; i < outputs.size(); i++) {
            Output& o = outputs[i];
            SDL_GL_MakeCurrent(o.win, ctx);
            SDL_GL_SetSwapInterval(i == 0 ? 1 : 0);
            int ow, oh;
            SDL_GL_GetDrawableSize(o.win, &ow, &oh);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            gfx.present(outputTexture(o.source), 0, 0, ow, oh, o.c, o.grid);
            if (i == 0 && !pendingOutShot.empty()) { captureBackbuffer(pendingOutShot, ow, oh); pendingOutShot.clear(); }
            SDL_GL_SwapWindow(o.win);
        }
        SDL_GL_MakeCurrent(win, ctx);
    }

    saveScenes();
    if (looksDirty) saveLooks();
    if (selftestDir.empty()) { std::error_code ec; fs::remove(fs::u8path(sessionPath()), ec); }
    djLink.stop();
    spoutRx.ReleaseReceiver();
    if (tempo.source() == Tempo::Source::External) tempo.setSource(Tempo::Source::Auto);
    S.tempoSource = (int)tempo.source();
    S.tapBpm = (float)tempo.bpm();
    S.link = tempo.linkEnabled();
    saveSettings();
    saveLibrary();
    stopRecording();
    video.close();
    if (spoutNamed) spout.ReleaseSender();
    osc.stop();
    web.stop();
    midi.close();
    thumbs.stop();
    audio.shutdown();
    ImGui::SetCurrentContext(overlayCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext(overlayCtx);
    ImGui::SetCurrentContext(uiCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(uiCtx);
    for (auto& d : decks)
        for (auto& p : d.players) p.stop();
    for (auto c : playerContexts) SDL_GL_DeleteContext(c);
    for (auto w : playerWindows) SDL_DestroyWindow(w);
    for (auto& o : outputs) SDL_DestroyWindow(o.win);
    if (thumbCtx) SDL_GL_DeleteContext(thumbCtx);
    SDL_DestroyWindow(thumbWin);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    CoUninitialize();
    return 0;
}
