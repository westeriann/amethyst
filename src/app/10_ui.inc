// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== UI helpers
const ImVec4 kAccent(0.70f, 0.53f, 1.0f, 1.0f);
const ImVec4 kAccent2(0.31f, 0.90f, 0.78f, 1.0f);
const ImVec4 kDim(0.62f, 0.60f, 0.70f, 1.0f);
const ImVec4 kWarn(1, 0.72f, 0.3f, 1);
const ImVec4 kHot(1, 0.36f, 0.48f, 1);

void setupStyle(float scale) {
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 10; st.ChildRounding = 8; st.FrameRounding = 6; st.PopupRounding = 8; st.GrabRounding = 6;
    st.TabRounding = 6; st.ScrollbarRounding = 8; st.WindowBorderSize = 1;
    st.WindowPadding = ImVec2(12, 10);
    st.ItemSpacing = ImVec2(8, 6);
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.08f, 0.82f);
    c[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.10f, 0.96f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.09f, 0.88f);
    c[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.09f);
    c[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.06f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.53f, 1.0f, 0.25f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.07f, 0.12f, 0.92f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.07f, 0.12f, 0.7f);
    c[ImGuiCol_Header] = ImVec4(0.70f, 0.53f, 1.0f, 0.22f);
    c[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.70f, 0.53f, 1.0f, 0.35f);
    c[ImGuiCol_Button] = ImVec4(1, 1, 1, 0.06f);
    c[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 0.12f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.53f, 1.0f, 0.35f);
    c[ImGuiCol_CheckMark] = kAccent2;
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = kAccent2;
    c[ImGuiCol_PlotHistogram] = kAccent2;
    c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_Tab] = ImVec4(1, 1, 1, 0.05f);
    c[ImGuiCol_TabSelected] = ImVec4(0.70f, 0.53f, 1.0f, 0.3f);
    c[ImGuiCol_TabHovered] = ImVec4(0.70f, 0.53f, 1.0f, 0.4f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.70f, 0.53f, 1.0f, 0.2f);
    st.ScaleAllSizes(scale);
}

std::vector<ThumbRenderer::Want> wantThumbs;
void wantThumb(int idx) {
    if (idx < 0 || lib[idx].broken) return;
    for (auto& w : wantThumbs)
        if (w.id == idx) return;
    wantThumbs.push_back({idx, lib[idx].path});
}

void imageFlipped(unsigned tex, ImVec2 size) { ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0, 1), ImVec2(1, 0)); }

void pill(const char* text, ImVec4 col, bool on, const char* tip, std::function<void()> onClick = nullptr) {
    ImGui::PushStyleColor(ImGuiCol_Button, on ? ImVec4(col.x, col.y, col.z, 0.22f) : ImVec4(1, 1, 1, 0.04f));
    ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(1, 1, 1, 1) : kDim);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 99);
    if (ImGui::SmallButton(text) && onClick) onClick();
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
}

std::string stars(int n) {
    std::string s;
    for (int i = 0; i < 5; i++) s += i < n ? "\xE2\x98\x85" : "\xE2\x98\x86";
    return s;
}

bool autoMenuTestOpen = false;
// Right-click menu on the AUTO button: every auto-pilot setting in one place.
std::vector<std::pair<std::string, std::string>> autoFolders;  // label, absolute path
size_t autoFoldersFor = (size_t)-1;
void buildAutoFolders() {
    autoFoldersFor = lib.size();
    std::map<std::string, std::string> dirs;  // path -> label
    for (auto& root : S.folders) {
        fs::path r = fs::u8path(root);
        std::string rootLabel = r.filename().u8string();
        if (rootLabel.empty()) rootLabel = root;
        for (auto& p : lib) {
            if (p.path.compare(0, root.size(), root) != 0) continue;
            fs::path d = fs::u8path(p.path).parent_path();
            while (d.u8string().size() >= root.size()) {
                std::string ds = d.u8string();
                if (dirs.count(ds)) break;
                std::string rel = ds.size() > root.size() ? ds.substr(root.size() + 1) : "";
                for (auto& c : rel) if (c == '\\') c = '/';
                dirs[ds] = rel.empty() ? rootLabel : rootLabel + "/" + rel;
                if (ds.size() == root.size()) break;
                d = d.parent_path();
            }
        }
    }
    autoFolders.clear();
    for (auto& [path, label] : dirs) autoFolders.push_back({label, path});
    std::sort(autoFolders.begin(), autoFolders.end());
}
std::string autoFolderLabel() {
    for (auto& f : autoFolders) if (f.second == S.autoFolder) return f.first;
    return fs::u8path(S.autoFolder).filename().u8string();
}
void setAutoFolder(const std::string& path) {
    S.autoFolder = path;
    rebuildAutoView();
    queueAnalysis();  // analyse that folder first
    upcoming = -1;
    saveSettings();
    if (path.empty()) toast(false, "Auto-pilot: any folder");
    else toast(false, "Auto-pilot stays in %s (%d presets)", autoFolderLabel().c_str(), (int)autoView.size());
}

void drawAutoMenu() {
    if (autoFoldersFor != lib.size()) buildAutoFolders();
    ImGui::TextColored(kAccent2, "Auto-pilot");
    if (ImGui::Checkbox("On", &S.autoOn)) decks[0].lastSwitch = decks[1].lastSwitch = now();
    ImGui::SameLine();
    ImGui::TextDisabled("(A)");
    ImGui::SeparatorText("Presets");
    {   // stay in folder
        static char filt[64] = "";
        std::string cur = S.autoFolder.empty() ? "Any folder" : autoFolderLabel();
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Stay in folder", cur.c_str(), ImGuiComboFlags_HeightLarge)) {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##ff", "filter folders...", filt, sizeof filt);
            if (ImGui::Selectable("Any folder", S.autoFolder.empty())) setAutoFolder("");
            std::string f = lower(filt);
            for (auto& [label, path] : autoFolders) {
                if (!f.empty() && lower(label).find(f) == std::string::npos) continue;
                if (ImGui::Selectable(label.c_str(), S.autoFolder == path)) setAutoFolder(path);
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto-pilot only picks presets from this folder (and its subfolders).\nThe browser filters (pool, folder, search) still narrow it further.");
        if (!S.autoFolder.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) setAutoFolder("");
            ImGui::TextDisabled("%d presets to pick from", (int)autoView.size());
        }
    }
    {   // journey: one mood for the whole set
        ImGui::SetNextItemWidth(260);
        std::string cur = journeyLabel();
        if (S.journey == kJourneyCount + 1) cur += std::string(" (now ") + kJourneys[wanderMood % kJourneyCount].name + ")";
        if (ImGui::BeginCombo("Journey", cur.c_str())) {
            if (ImGui::Selectable("Off", S.journey == 0)) S.journey = 0;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("No mood: presets and effects follow the song only.");
            ImGui::Separator();
            for (int i = 0; i < kJourneyCount; i++) {
                if (ImGui::Selectable(kJourneys[i].name, S.journey == i + 1)) S.journey = i + 1, vibe.set = false;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kJourneys[i].desc);
            }
            ImGui::Separator();
            if (ImGui::Selectable("Wander", S.journey == kJourneyCount + 1)) S.journey = kJourneyCount + 1, wanderNext(), vibe.set = false;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Travel through the moods: a new, related mood every two songs\n(or every six minutes in a long mix).");
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("A mood for the whole set. Auto-pilot picks presets that match it\n(colour, brightness, movement) and Auto FX favours effects that fit.");
    }
    {   // smart picking needs the analysis: say so when most of the pool is unknown
        const auto& pool = S.autoFolder.empty() || autoView.empty() ? view : autoView;
        int known = 0;
        for (int i : pool) known += looks.count(lib[i].path) ? 1 : 0;
        if (!pool.empty() && known < (int)pool.size() * 0.8) {
            ImGui::TextColored(ImVec4(1.f, 0.75f, 0.35f, 1.f), "Only %d of %d presets here are analysed.", known, (int)pool.size());
            ImGui::TextDisabled("Picks match the moment best once presets are analysed.");
            if (!analyseNow && S.analysisOn) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Analyse now")) analyseNow = true;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Runs while you play too (small hitches possible), so best before the show.\nThis folder goes first. It stops by itself when done.");
            }
        }
    }
    ImGui::Checkbox("Shuffle", &S.shuffle);
    ImGui::SameLine();
    ImGui::TextDisabled(S.shuffle ? "random order" : "in list order");
    ImGui::Checkbox("Mix decks A/B", &S.autoDecks);
    ImGui::SetNextItemWidth(160);
    ImGui::SliderInt("Change every (s)", &S.interval, 4, 300);
    ImGui::SeparatorText("Following the song");
    ImGui::Checkbox("Smart mode (switch on phrases and drops)", &S.smartMix);
    ImGui::BeginDisabled(!S.smartMix);
    ImGui::Checkbox("New preset on the real drop after a fakeout", &S.fakeoutRecut);
    ImGui::Checkbox("Smart blend modes on drops", &S.smartBlends);
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Build-up tease", &S.buildTease, 0.f, 0.6f, "%.2f");
    ImGui::EndDisabled();
    if (!S.smartMix) {
        ImGui::SetNextItemWidth(160);
        ImGui::Combo("Launch on", &S.quantize, kQuantizeNames, 4);
    }
    ImGui::Checkbox("Calmer while vocals", &S.vocalEase);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("While someone sings: softer Auto FX, no phrase flashes,\nand switches wait for the vocal line to finish.");
    ImGui::SameLine();
    ImGui::TextDisabled(structure.vocalsOn() ? "(vocals now)" : "");
    ImGui::SeparatorText("Auto FX");
    ImGui::Checkbox("Effects follow the song", &S.autoFx);
    ImGui::BeginDisabled(!S.autoFx);
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Strength", &S.autoFxAmount, 0.f, 1.f, "%.2f");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Groove texture", &S.autoTexture, 0.f, 1.f, "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Small moves on hi-hats and snares: shimmer, sway, breathing, echoes, colour steps.\n"
                          "Strongest in breakdowns, intros and low-bass grooves (house, garage), light on drops.");
    if (afx.texAmt > 0.01f) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", kTexNames[afx.tex]);
    }
    ImGui::Checkbox("Flashes & strobes", &S.autoFxFlash);
    ImGui::Checkbox("Use custom shaders", &S.autoFxShaders);
    ImGui::EndDisabled();
    ImGui::SeparatorText("Safety");
    ImGui::Checkbox("Show safety watchdog", &S.watchdog);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skips presets that go black, freeze or run too slowly while music plays.");
    {
        auto it = byPath.find(S.safePreset);
        ImGui::TextDisabled("Safe preset: %s", it != byPath.end() ? lib[it->second].name.c_str() : "(automatic)");
    }
    if (ImGui::Button("Safe preset now")) panicSafe();
    ImGui::SameLine();
    ImGui::TextDisabled("right-click a preset to choose it");
}

void presetContextMenu(int idx) {
    Preset& p = lib[idx];
    Meta& m = meta[p.path];
    ImGui::TextDisabled("%s", p.name.c_str());
    if (ImGui::MenuItem("Load to deck A")) loadPreset(0, idx, false);
    if (ImGui::MenuItem("Load to deck B")) loadPreset(1, idx, false);
    mappingContextMenu(idx);
    ImGui::Separator();
    if (ImGui::MenuItem("Favorite", "Ctrl+D", m.fav)) { m.fav = !m.fav; saveLibrary(); }
    if (ImGui::MenuItem("Use as safe preset", nullptr, S.safePreset == p.path)) {
        S.safePreset = p.path;
        saveSettings();
        toast(false, "Safe preset: %s (used by show safety and the panic button)", p.name.c_str());
    }
    if (ImGui::BeginMenu("Rating")) {
        for (int r = 0; r <= 5; r++)
            if (ImGui::MenuItem(r ? stars(r).c_str() : "No rating", nullptr, m.rating == r)) { m.rating = r; saveLibrary(); }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tags")) {
        for (size_t i = 0; i < m.tags.size(); i++)
            if (ImGui::MenuItem(("Remove \"" + m.tags[i] + "\"").c_str())) { m.tags.erase(m.tags.begin() + i); saveLibrary(); break; }
        for (auto& t : allTags())
            if (std::find(m.tags.begin(), m.tags.end(), t) == m.tags.end() && ImGui::MenuItem(("Add \"" + t + "\"").c_str())) { m.tags.push_back(t); saveLibrary(); }
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputTextWithHint("##newtag", "new tag + Enter", tagBuf, sizeof tagBuf, ImGuiInputTextFlags_EnterReturnsTrue) && tagBuf[0]) {
            m.tags.push_back(tagBuf);
            tagBuf[0] = 0;
            saveLibrary();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Add to setlist")) {
        for (auto& s : setlists)
            if (ImGui::MenuItem(s.name.c_str())) addToSetlist(idx, s.name);
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputTextWithHint("##newsl", "new setlist + Enter", setlistNameBuf, sizeof setlistNameBuf, ImGuiInputTextFlags_EnterReturnsTrue) && setlistNameBuf[0]) {
            addToSetlist(idx, setlistNameBuf);
            setlistNameBuf[0] = 0;
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    for (int c = 0; c < 10; c++) {
        char lbl[32];
        snprintf(lbl, sizeof lbl, "Store in cue %d", (c + 1) % 10);
        if (ImGui::MenuItem(lbl)) { S.cues[c] = p.path; saveSettings(); }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("More like this", nullptr, false, looks.count(p.path) > 0)) setPool("similar:" + p.path);
    if (ImGui::MenuItem("Edit preset")) { loadPreset(focus, idx, true); openEditor(); }
    if (ImGui::MenuItem("Show in Explorer")) plat::revealInExplorer(p.path);
    if (p.broken && ImGui::MenuItem("Retry")) { p.broken = false; brokenPaths.erase(p.path); }
}

// ===================================================================== panels
void drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Add preset files...", "Ctrl+O")) dialogAddFiles();
        if (ImGui::MenuItem("Add preset folder...", "Ctrl+Shift+O")) dialogAddFolder();
        if (ImGui::MenuItem("Rescan library", "Ctrl+R")) rescan();
        if (ImGui::BeginMenu("Library folders", !S.folders.empty())) {
            for (size_t i = 0; i < S.folders.size(); i++) {
                ImGui::PushID((int)i);
                if (ImGui::MenuItem(("Remove  " + S.folders[i]).c_str())) {
                    S.folders.erase(S.folders.begin() + i);
                    rescan();
                    saveSettings();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Forget single files", nullptr, false, !S.files.empty())) { S.files.clear(); rescan(); saveSettings(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Play audio file...", "M")) playAudioFileDialog();
        if (ImGui::MenuItem("Preset editor", "E")) openEditor();
        ImGui::Separator();
        if (ImGui::MenuItem("Open recordings folder")) { std::string d = plat::videosDir() + "\\Amethyst"; fs::create_directories(fs::u8path(d)); plat::openPath(d); }
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Audio")) {
        refreshDevices();
        ImGui::TextDisabled("System audio (what you hear)");
        for (auto& d : audioDevices)
            if (d.loopback && ImGui::MenuItem((d.name + "##lb").c_str(), d.isDefault ? "default" : nullptr,
                                              audio.source() == AudioEngine::Source::Loopback && audio.deviceName() == d.name))
                startAudioFrom("loopback", d.name, false);
        ImGui::Separator();
        ImGui::TextDisabled("Microphone / line-in");
        for (auto& d : audioDevices)
            if (!d.loopback && ImGui::MenuItem((d.name + "##in").c_str(), d.isDefault ? "default" : nullptr,
                                               audio.source() == AudioEngine::Source::Input && audio.deviceName() == d.name))
                startAudioFrom("input", d.name, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Audio file...", "M", audio.source() == AudioEngine::Source::File)) playAudioFileDialog();
        if (ImGui::MenuItem("Pause / resume file", "Shift+M", false, audio.source() == AudioEngine::Source::File)) audio.togglePause();
        if (ImGui::MenuItem("No audio", nullptr, audio.source() == AudioEngine::Source::None)) startAudioFrom("none", "", false);
        ImGui::Separator();
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Sensitivity", &S.gain, 0.05f, 8.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Output")) {
        int n = SDL_GetNumVideoDisplays();
        for (int i = 0; i < n; i++) {
            SDL_Rect b;
            SDL_GetDisplayBounds(i, &b);
            char label[256];
            snprintf(label, sizeof label, "New fullscreen output on %d: %s (%dx%d)", i + 1, SDL_GetDisplayName(i), b.w, b.h);
            if (ImGui::MenuItem(label)) openOutput(i);
        }
        if (ImGui::MenuItem("New windowed output")) openOutput(-1);
        if (ImGui::MenuItem("Outputs & corner pin...", nullptr, S.showOutputs)) S.showOutputs = !S.showOutputs;
        if (ImGui::MenuItem("Projection mapping (surfaces)...", nullptr, S.showMapping)) S.showMapping = !S.showMapping;
        ImGui::Separator();
        if (ImGui::MenuItem("Spout sender (to OBS, Resolume...)", "F12", S.spout)) setSpout(!S.spout);
        if (ImGui::MenuItem(recorder.active() ? "Stop recording" : "Record to MP4", "F9", recorder.active())) actions.invoke("record", true);
        if (ImGui::MenuItem("Render audio file to video...", nullptr, false, !rj.active && !recorder.active())) openRenderDialog();
        if (ImGui::BeginMenu("Recording frame rate")) {
            for (int f : {24, 30, 60})
                if (ImGui::MenuItem((std::to_string(f) + " fps").c_str(), nullptr, S.recFps == f, !recorder.active())) S.recFps = f;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        for (int q = 0; q < 4; q++)
            if (ImGui::MenuItem(kQuality[q].label, q == S.quality ? "Q" : nullptr, S.quality == q)) {
                S.quality = q;
                for (auto& d : decks) for (auto& p : d.players) p.setMesh(kQuality[q].meshW, kQuality[q].meshH);
                mappingSetMesh(kQuality[q].meshW, kQuality[q].meshH);
            }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Control")) {
        if (ImGui::MenuItem("MIDI, OSC & phone remote...", nullptr, S.showControl)) S.showControl = !S.showControl;
        ImGui::Separator();
        if (ImGui::BeginMenu("Tempo source")) {
            if (ImGui::MenuItem("Automatic (detect from audio)", nullptr, tempo.source() == Tempo::Source::Auto)) tempo.setSource(Tempo::Source::Auto);
            if (ImGui::MenuItem("Tap tempo", "T", tempo.source() == Tempo::Source::Tap)) tempo.setSource(Tempo::Source::Tap);
            if (ImGui::MenuItem("Ableton Link", nullptr, tempo.source() == Tempo::Source::Link)) { tempo.enableLink(true); S.link = true; tempo.setSource(Tempo::Source::Link); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Launch quantize")) {
            for (int q = 0; q < 4; q++)
                if (ImGui::MenuItem(kQuantizeNames[q], q == S.quantize ? "K" : nullptr, S.quantize == q)) S.quantize = q;
            ImGui::Separator();
            ImGui::MenuItem("Also quantize manual launches", nullptr, &S.quantizeManual);
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Auto-pilot mixes decks A/B", nullptr, &S.autoDecks);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Menu bar", "F10", &S.showMenu);
        ImGui::MenuItem("Preset browser", "P", &S.showBrowser);
        ImGui::MenuItem("Now-playing bar", "N", &S.showHud);
        ImGui::MenuItem("Cue strip", "C", &S.showCues);
        ImGui::MenuItem("Mixer (decks A/B)", "D", &S.showMixer);
        ImGui::MenuItem("Effects", "W", &S.showFx);
        ImGui::MenuItem("Overlays (logo, text, video)", nullptr, &S.showOverlays);
        ImGui::MenuItem("Outputs & mapping", nullptr, &S.showOutputs);
        ImGui::MenuItem("Projection mapping", nullptr, &S.showMapping);
        ImGui::MenuItem("Setlists", nullptr, &S.showSetlists);
        ImGui::MenuItem("Scenes & show timeline", nullptr, &S.showScenes);
        ImGui::MenuItem("Modulation", nullptr, &S.showMod);
        ImGui::MenuItem("Control (MIDI, OSC, remote)", nullptr, &S.showControl);
        ImGui::MenuItem("Notifications", nullptr, &S.showToasts);
        ImGui::MenuItem("Status in menu bar", nullptr, &S.showStatus);
        ImGui::Separator();
        ImGui::MenuItem("Live previews", "V", &S.livePreviews);
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("Preview size", &S.previewHeight, 36, 160, "%.0f px");
        ImGui::SetNextItemWidth(160);
        if (ImGui::SliderInt("Preview FPS", &S.previewFps, 5, 60)) thumbs.setFps(S.previewFps);
        ImGui::Separator();
        if (ImGui::MenuItem("Hide entire interface", "U")) toggleAllUi();
        if (ImGui::MenuItem("Fullscreen", "F")) toggleFullscreen(win);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Keyboard shortcuts", chordsFor("help").empty() ? nullptr : chordsFor("help")[0].c_str())) helpOpen = true;
        if (ImGui::MenuItem("Customize keyboard layout...", chordsFor("keyboard").empty() ? nullptr : chordsFor("keyboard")[0].c_str())) keyboardOpen = true;
        if (ImGui::MenuItem("User guide")) plat::openPath("https://github.com/westeriann/amethyst/blob/main/docs/GUIDE.md");
        ImGui::TextDisabled("projectM %s", projectm_get_version_string());
        ImGui::EndMenu();
    }
    if (S.showStatus) {
        char st[200];
        snprintf(st, sizeof st, "%s%d presets   %.0f fps   %dx%d", recorder.active() ? "REC  " : "", (int)lib.size(), ImGui::GetIO().Framerate, renderW, renderH);
        float w = ImGui::CalcTextSize(st).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - w - 16);
        ImGui::TextColored(recorder.active() ? kHot : kDim, "%s", st);
    }
    ImGui::EndMainMenuBar();
}

void drawBrowser(float top, float bottom) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 12, top), ImGuiCond_FirstUseEver, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(460 * io.FontGlobalScale, std::max(200.f, bottom - top)), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Presets", &S.showBrowser, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

    if (focusSearch) { ImGui::SetKeyboardFocusHere(); focusSearch = false; }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search presets...  ( / )", searchBuf, sizeof searchBuf)) { query = searchBuf; rebuildView(); }
    searchHasFocus = ImGui::IsItemActive();

    float half = (ImGui::GetContentRegionAvail().x - 8) * 0.5f;
    ImGui::SetNextItemWidth(half);
    if (ImGui::BeginCombo("##pool", poolLabel().c_str())) {
        if (ImGui::Selectable("All presets", S.pool == "all")) setPool("all");
        if (ImGui::Selectable("Favorites", S.pool == "fav")) setPool("fav");
        ImGui::SeparatorText("By look (auto-analysed)");
        for (auto c : kColorNames)
            if (ImGui::Selectable((std::string("Colour: ") + c).c_str(), S.pool == std::string("color:") + c)) setPool(std::string("color:") + c);
        if (ImGui::Selectable("Calm", S.pool == "energy:calm")) setPool("energy:calm");
        if (ImGui::Selectable("Intense", S.pool == "energy:intense")) setPool("energy:intense");
        if (ImGui::Selectable("Dark", S.pool == "light:dark")) setPool("light:dark");
        if (ImGui::Selectable("Bright", S.pool == "light:bright")) setPool("light:bright");
        if (S.pool.rfind("similar:", 0) == 0) ImGui::Selectable(poolLabel().c_str(), true);
        ImGui::SeparatorText("Library");
        for (int r = 3; r <= 5; r++)
            if (ImGui::Selectable((stars(r) + (r < 5 ? " and up" : "")).c_str(), S.pool == "rating:" + std::to_string(r))) setPool("rating:" + std::to_string(r));
        for (auto& t : allTags())
            if (ImGui::Selectable(("Tag: " + t).c_str(), S.pool == "tag:" + t)) setPool("tag:" + t);
        for (auto& s : setlists)
            if (ImGui::Selectable(("Setlist: " + s.name).c_str(), S.pool == "setlist:" + s.name)) setPool("setlist:" + s.name);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(half);
    if (ImGui::BeginCombo("##src", sourceFilter < 0 ? "All folders" : sourceNames[sourceFilter].c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("All folders", sourceFilter < 0)) { sourceFilter = -1; rebuildView(); }
        for (int i = 0; i < (int)sourceNames.size(); i++)
            if (ImGui::Selectable(sourceNames[i].c_str(), sourceFilter == i)) { sourceFilter = i; rebuildView(); }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Live previews", &S.livePreviews);
    ImGui::SameLine();
    if (ImGui::Checkbox("Hide broken", &S.hideBroken)) rebuildView();
    ImGui::SameLine();
    {
        int pend = thumbs.analysisPending();
        if (pend > 0 && S.analysisOn) {
            // progress + time left, from the rate measured while it was running
            static int lastPend = -1;
            static double rate = 0, lastT = 0;
            double tn = now();
            if (lastPend < 0 || pend > lastPend) lastPend = pend, lastT = tn;
            else if (tn - lastT > 5) {
                double r = (lastPend - pend) / (tn - lastT);
                if (thumbs.analysisEnabled && r > 0) rate = rate > 0 ? rate * 0.7 + r * 0.3 : r;
                lastPend = pend, lastT = tn;
            }
            int total = std::max(pend, analysisQueued);
            ImGui::SetNextItemWidth(110);
            char ov[64];
            if (rate > 0) snprintf(ov, sizeof ov, "%d left, ~%d min", pend, (int)std::ceil(pend / rate / 60));
            else snprintf(ov, sizeof ov, "%d left", pend);
            ImGui::ProgressBar(total ? 1.f - pend / (float)total : 0.f, ImVec2(170, 0), ov);
        } else {
            ImGui::TextDisabled("%d analysed", (int)looks.size());
        }
        if (pend > 0 && S.analysisOn && !thumbs.analysisEnabled && !analyseNow) {
            ImGui::SameLine();
            ImGui::TextDisabled("(paused while performing)");
            ImGui::SameLine();
            if (ImGui::SmallButton("Analyse now")) analyseNow = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run the analysis even while performing. It can cause small hitches,\nso use it when the show is not live. Turns itself off when finished.");
        } else if (analyseNow) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Pause analysis")) analyseNow = false;
            if (pend == 0) analyseNow = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Presets are analysed in the background (colour, brightness, motion)\nfor the colour/energy pools and \"More like this\". Right-click to pause.");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { S.analysisOn = !S.analysisOn; thumbs.analysisEnabled = S.analysisOn; }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d shown  -  loads into deck %s%s", (int)view.size(), decks[focus].name, keyHint("focus_toggle").c_str());

    if (lib.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("No presets yet. Drop .milk files or a whole folder onto the window, or:");
        if (ImGui::Button("Add preset folder...")) dialogAddFolder();
        ImGui::End();
        return;
    }

    const float th = S.previewHeight * io.FontGlobalScale;
    const float tw = th * 16.f / 9.f;
    const float rowH = th + 6;
    ImGui::BeginChild("list", ImVec2(0, 0), ImGuiChildFlags_None);
    if (scrollToSel && sel >= 0) {
        float y = sel * rowH, scr = ImGui::GetScrollY(), h = ImGui::GetWindowHeight();
        if (y < scr) ImGui::SetScrollY(y);
        else if (y + rowH > scr + h) ImGui::SetScrollY(y + rowH - h);
        scrollToSel = false;
    }
    ImGuiListClipper clip;
    clip.Begin((int)view.size(), rowH);
    while (clip.Step()) {
        for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
            int idx = view[r];
            Preset& p = lib[idx];
            ImGui::PushID(idx);
            ImVec2 rowPos = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##row", r == sel, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowH - 2))) {
                sel = r;
                if (io.KeyCtrl) loadPreset(1 - focus, idx, io.KeyShift);
                else loadPreset(focus, idx, io.KeyShift);
            }
            if (ImGui::BeginPopupContextItem()) { presetContextMenu(idx); ImGui::EndPopup(); }
            if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s\nClick: deck %s   Ctrl+click: deck %s   Shift: cut", p.path.c_str(), decks[focus].name, decks[1 - focus].name);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 tp(rowPos.x + 2, rowPos.y + 2), te(tp.x + tw, tp.y + th - 2);
            unsigned tex = S.livePreviews ? thumbs.textureFor(idx) : 0;
            if (tex) dl->AddImageRounded((ImTextureID)(intptr_t)tex, tp, te, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, 5);
            else dl->AddRectFilled(tp, te, IM_COL32(255, 255, 255, 12), 5);
            wantThumb(idx);
            bool onA = idx == decks[0].cur, onB = idx == decks[1].cur;
            ImU32 col = p.broken ? IM_COL32(130, 126, 150, 255) : (onA || onB) ? ImGui::GetColorU32(kAccent2) : IM_COL32(236, 235, 245, 255);
            float tx = rowPos.x + tw + 12;
            float lh = ImGui::GetTextLineHeight();
            ImVec4 clipR(tx, rowPos.y, rowPos.x + ImGui::GetWindowContentRegionMax().x - 4, rowPos.y + rowH);
            dl->AddText(nullptr, 0, ImVec2(tx, rowPos.y + rowH * 0.5f - lh), col, p.name.c_str(), nullptr, 0, &clipR);
            auto mi = meta.find(p.path);
            std::string info;
            if (onA) info += "[A] ";
            if (onB) info += "[B] ";
            if (mi != meta.end()) {
                if (mi->second.fav) info += "\xE2\x99\xA5 ";
                if (mi->second.rating) info += stars(mi->second.rating) + " ";
                for (auto& t : mi->second.tags) info += "#" + t + " ";
            }
            info += p.source;
            dl->AddText(nullptr, 0, ImVec2(tx, rowPos.y + rowH * 0.5f + 1), ImGui::GetColorU32(kDim), info.c_str(), nullptr, 0, &clipR);
            for (int c = 0; c < 10; c++)
                if (S.cues[c] == p.path) {
                    char b[4];
                    snprintf(b, sizeof b, "%d", (c + 1) % 10);
                    dl->AddText(ImVec2(rowPos.x + 6, rowPos.y + 4), ImGui::GetColorU32(kAccent), b);
                }
            ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + rowH));
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void drawCues(float y, float h) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12, y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 24, h), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cues", &S.showCues, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) { ImGui::End(); return; }
    float avail = ImGui::GetContentRegionAvail().x;
    float cw = (avail - 9 * 6) / 10.f;
    float chh = std::max(24.f, ImGui::GetContentRegionAvail().y - 2);
    for (int i = 0; i < 10; i++) {
        ImGui::PushID(i);
        auto it = S.cues[i].empty() ? byPath.end() : byPath.find(S.cues[i]);
        int idx = it == byPath.end() ? -1 : it->second;
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("cue", ImVec2(cw, chh))) {
            if (io.KeyShift) storeCue(i);
            else if (io.KeyAlt) clearCue(i);
            else fireCue(i);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) clearCue(i);
        if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s", idx >= 0 ? lib[idx].name.c_str() : "Empty - Shift+click or Shift+number stores the current preset");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool active = idx >= 0 && (idx == decks[0].cur || idx == decks[1].cur);
        dl->AddRectFilled(p, ImVec2(p.x + cw, p.y + chh), active ? IM_COL32(179, 136, 255, 60) : IM_COL32(255, 255, 255, 10), 7);
        float textX = p.x + 22;
        if (idx >= 0) {
            unsigned tex = S.livePreviews ? thumbs.textureFor(idx) : 0;
            float ih = chh - 4, iw = std::min(cw * 0.45f, ih * 16 / 9.f);
            if (tex) dl->AddImageRounded((ImTextureID)(intptr_t)tex, ImVec2(p.x + 18, p.y + 2), ImVec2(p.x + 18 + iw, p.y + 2 + ih), ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, 5);
            wantThumb(idx);
            textX = p.x + 24 + iw;
            ImVec4 clipR(textX, p.y, p.x + cw - 4, p.y + chh);
            dl->AddText(nullptr, 0, ImVec2(textX, p.y + chh * 0.5f - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(236, 235, 245, 255),
                        lib[idx].name.c_str(), nullptr, 0, &clipR);
        }
        char num[4];
        snprintf(num, sizeof num, "%d", (i + 1) % 10);
        dl->AddText(ImVec2(p.x + 6, p.y + 3), ImGui::GetColorU32(kAccent), num);
        if (active) dl->AddRect(p, ImVec2(p.x + cw, p.y + chh), ImGui::GetColorU32(kAccent), 7, 0, 1.5f);
        if (i < 9) ImGui::SameLine(0, 6);
        ImGui::PopID();
    }
    ImGui::End();
}

void drawHud(float y, float h) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12, y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 24, h), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Now playing", &S.showHud, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) { ImGui::End(); return; }

    float rightW = 340 * io.FontGlobalScale;
    int fc = decks[focus].cur;
    ImGui::BeginGroup();
    ImGui::PushFont(fontBig);
    ImGui::TextUnformatted(fc >= 0 ? lib[fc].name.c_str() : "No preset");
    ImGui::PopFont();
    int other = decks[1 - focus].cur;
    auto it = std::find(view.begin(), view.end(), fc);
    std::string sub = "Deck " + std::string(decks[focus].name);
    if (fc >= 0 && it != view.end()) sub += "  #" + std::to_string(it - view.begin() + 1) + " of " + std::to_string(view.size());
    if (S.pool != "all") sub += "  -  " + poolLabel();
    if (!query.empty()) sub += "  -  search: " + query;
    if (S.xfade > 0.02f && other >= 0) sub += "   |   deck " + std::string(decks[1 - focus].name) + ": " + lib[other].name;
    ImGui::TextDisabled("%s", sub.c_str());
    ImGui::EndGroup();

    // tempo + level (right side)
    ImGui::SameLine(ImGui::GetWindowWidth() - rightW - 12);
    ImGui::BeginGroup();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    double beat = beatNow();
    int bi = ((int)std::floor(beat) % 4 + 4) % 4;
    float frac = (float)(beat - std::floor(beat));
    ImVec2 p = ImGui::GetCursorScreenPos();
    for (int i = 0; i < 4; i++) {
        ImU32 c = i == bi ? IM_COL32(255, 110, 199, (int)(255 - 180 * frac)) : IM_COL32(255, 255, 255, 30);
        dl->AddCircleFilled(ImVec2(p.x + 7 + i * 16, p.y + 9), 6, c);
    }
    ImGui::Dummy(ImVec2(66, 18));
    ImGui::SameLine();
    char bpm[64];
    snprintf(bpm, sizeof bpm, "%.1f BPM  %s%s", tempo.bpm(), tempo.sourceName(),
             tempo.source() == Tempo::Source::Link ? (" (" + std::to_string(tempo.linkPeers()) + " peers)").c_str() : "");
    if (ImGui::SmallButton(bpm)) tempo.tap(now());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click or press T to tap.  Shift+T: Auto / Tap / Link");
    ImGui::ProgressBar(std::clamp(level * 3.f, 0.f, 1.f), ImVec2(rightW - 10, 8), "");
    ImGui::TextDisabled("%s%s", audio.label().c_str(), audio.paused() ? "  (paused)" : "");
    ImGui::EndGroup();

    char b[128];
    double since = now() - decks[S.xfade < 0.5f ? 0 : 1].lastSwitch;
    double left = std::max(0.0, S.interval - since);
    double hb = beatNow();
    if (S.autoOn && S.smartMix)
        snprintf(b, sizeof b, "AUTO smart  %s%s  bar %d/8%s%s%s%s", MusicStructure::name(structure.section()), structure.vocalsOn() ? " + vox" : "",
                 structure.barInPhrase(hb) + 1,
                 S.autoFx ? " +FX" : "", S.autoDecks ? ", A/B mix" : "", S.autoFolder.empty() ? "" : ("  in " + fs::u8path(S.autoFolder).filename().u8string()).c_str(),
                 keyHint("auto").c_str());
    else if (S.autoOn) snprintf(b, sizeof b, "AUTO %s  %s%s%s", locked ? "--" : (std::to_string((int)std::ceil(left)) + "s").c_str(), S.shuffle ? "shuffle" : "in order", S.autoDecks ? ", A/B mix" : "", keyHint("auto").c_str());
    else snprintf(b, sizeof b, "AUTO off%s", keyHint("auto").c_str());
    pill(b, kAccent2, S.autoOn, "Auto-pilot (A). Right-click for options: folder, smart mode, Auto FX...", [] { actions.invoke("auto", true); });
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || autoMenuTestOpen) ImGui::OpenPopup("##automenu"), autoMenuTestOpen = false;
    if (ImGui::BeginPopup("##automenu")) {
        drawAutoMenu();
        ImGui::EndPopup();
    }
    if (locked) pill(("LOCKED" + keyHint("lock")).c_str(), kWarn, true, nullptr, [] { locked = false; });
    snprintf(b, sizeof b, "QUANTIZE %s%s", kQuantizeNames[S.quantize], keyHint("quantize").c_str()); pill(b, kAccent, S.quantize > 0, "Launches wait for the next beat/bar", [] { actions.invoke("quantize", true); });
    snprintf(b, sizeof b, "BLEND %.2gs [ ]", S.blend); pill(b, kAccent, false, "Blend time");
    snprintf(b, sizeof b, "EVERY %ds , .", S.interval); pill(b, kAccent, false, "Auto-pilot interval");
    snprintf(b, sizeof b, "MIX %s %.0f%%%s", mixModeName(S.mixMode), S.xfade * 100, keyHint("mix_mode").c_str()); pill(b, kAccent, S.xfade > 0.01f, "Crossfader: Ctrl+Left/Right, \\ auto-fades", [] { actions.invoke("xfade_auto", true); });
    snprintf(b, sizeof b, "SPEED %.2gx", S.speed); pill(b, kWarn, S.speed != 1, "- = (Backspace resets)", [] { S.speed = 1; });
    snprintf(b, sizeof b, "GAIN %.2gx", S.gain); pill(b, kAccent, false, "Audio sensitivity (Up/Down)");
    snprintf(b, sizeof b, "%s%s", kQuality[S.quality].label, keyHint("quality").c_str()); pill(b, kAccent, false, "Render quality");
    int fxOn = S.fx.kaleido + S.fx.mirror + S.fx.feedback + S.fx.rgbSplit + S.fx.pixelate + S.fx.invert + S.fx.strobe;
    if (fxOn) { snprintf(b, sizeof b, "FX x%d%s", fxOn, keyHint("panel_fx").c_str()); pill(b, kAccent2, true, "Effects panel (W)", [] { S.showFx = true; }); }
    if (!outputs.empty()) { snprintf(b, sizeof b, "OUTPUT x%d", (int)outputs.size()); pill(b, kAccent2, true, nullptr, [] { S.showOutputs = true; }); }
    if (S.spout) pill("SPOUT", kAccent2, true, nullptr, [] { setSpout(false); });
    if (recorder.active()) { snprintf(b, sizeof b, "REC %02d:%02d%s", (int)recorder.seconds() / 60, (int)recorder.seconds() % 60, keyHint("record").c_str()); pill(b, kHot, true, nullptr, [] { stopRecording(); }); }
    if (blackoutOn) pill(("BLACKOUT" + keyHint("blackout")).c_str(), kHot, true, nullptr, [] { blackoutOn = false; });
    if (frozen) pill(("FROZEN" + keyHint("freeze")).c_str(), kWarn, true, nullptr);
    if (launch.armed) pill("LAUNCH QUEUED", kWarn, true, "Waiting for the next beat/bar");
    if (!midi.learning.empty()) pill("MIDI LEARN...", kWarn, true, "Move a control on your MIDI device (Esc cancels)");
    ImGui::NewLine();
    ImGui::End();
}

void deckColumn(int d, float w) {
    Deck& dk = decks[d];
    ImGui::BeginGroup();
    ImGui::PushID(d);
    bool foc = focus == d;
    ImGui::TextColored(foc ? kAccent2 : kDim, "DECK %s%s", dk.name, foc ? "  (focus)" : "");
    float h = w * 9 / 16.f;
    if (dk.outTex) imageFlipped(dk.outTex, ImVec2(w, h));
    else ImGui::Dummy(ImVec2(w, h));
    if (ImGui::IsItemClicked()) focus = d;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextUnformatted(dk.cur >= 0 ? lib[dk.cur].name.c_str() : "-");
    ImGui::PopTextWrapPos();
    if (ImGui::Button("< Prev")) back(d, false);
    ImGui::SameLine();
    if (ImGui::Button("Next >")) go(d, 1, false);
    ImGui::SameLine();
    if (ImGui::Button("Random")) go(d, 1, false, true);
    ImGui::SameLine();
    if (ImGui::Button("Focus")) focus = d;
    ImGui::PopID();
    ImGui::EndGroup();
}

void drawLightingTab();
void drawDjTab();
void drawLayerSource();
bool selectDjTab = false;

void drawMixer() {
    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Mixer", &S.showMixer, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    float w = (ImGui::GetContentRegionAvail().x - 16) / 2;
    deckColumn(0, w);
    ImGui::SameLine(0, 16);
    deckColumn(1, w);
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##xf", &S.xfade, 0, 1, "A  <-  crossfader  ->  B")) xanim.active = false;
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Mix mode", mixModeName(S.mixMode))) {
        for (int m = 0; m < MixModeCount; m++)
            if (ImGui::Selectable(mixModeName(m), S.mixMode == m)) S.mixMode = m;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto crossfade  ( \\ )")) actions.invoke("xfade_auto", true);
    ImGui::Checkbox("Auto-pilot mixes A/B (loads the next preset on the hidden deck, then crossfades)", &S.autoDecks);
    ImGui::Checkbox("Smart auto-mix: follow the song", &S.smartMix);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Switches on phrase boundaries, cuts to an intense preset exactly on the drop,\n"
                          "goes calm with a long blend in breakdowns, and changes when a new song starts.");
    if (S.smartMix) {
        ImGui::Checkbox("After a fakeout, switch again on the real drop", &S.fakeoutRecut);
        ImGui::SetNextItemWidth(140);
        ImGui::SliderFloat("Build-up tease", &S.buildTease, 0.f, 0.6f, "%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("During a build-up the drop preset slowly fades in up to this much,\nthen snaps to full on the drop. 0 = off.");
        ImGui::Checkbox("Smart blend modes", &S.smartBlends);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("With Auto FX: on a drop both presets slam together for a bar (add, screen, difference or\n"
                              "lighten) before the old one melts away, and build-up teases glow in instead of fading.");
        ImGui::Checkbox("Auto FX: effects follow the song", &S.autoFx);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("While auto-pilot runs: flash, zoom punch and strobe on drops, risers in build-ups,\n"
                              "dark tension in fakeout gaps, soft trails in breakdowns, a new effect look every phrase,\n"
                              "fades between songs and matching transition styles. Adds on top of your own FX.");
        if (S.autoFx) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140);
            ImGui::SliderFloat("strength##afx", &S.autoFxAmount, 0.f, 1.f, "%.2f");
            ImGui::Checkbox("Auto FX flashes & strobes", &S.autoFxFlash);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off: Auto FX never flashes white, strobes or flips colours.\nEverything else (zoom, trails, mirrors...) still follows the song.");
            ImGui::TextDisabled("Auto FX look: %s   texture: %s (%.0f%%)   hats/beat %.1f", kLookNames[afx.look], kTexNames[afx.tex], afx.texAmt * 100,
                                structure.hatsPerBeat());
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Some songs drop for a bar or two, cut out, then drop for real.\nOn: the real drop gets its own fresh preset. Off: keep the one from the fake drop.");
        double hb = beatNow();
        ImGui::TextDisabled("Now: %s   bar %d/8, beat %d   energy %.2f   last: %s", MusicStructure::name(structure.section()),
                            structure.barInPhrase(hb) + 1, structure.beatInBar(hb) + 1, structure.energy(),
                            smart.lastEvent.empty() ? "-" : smart.lastEvent.c_str());
    }
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("Transition", transitionName(S.transition))) {
        for (int k = 0; k < TransitionCount; k++)
            if (ImGui::Selectable(transitionName(k), S.transition == k)) S.transition = k;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170);
    ImGui::SliderFloat("##tbeats", &S.transBeats, 0, 32, S.transBeats < 0.01f ? "length: blend time" : "length: %.0f beats");
    ImGui::TextDisabled("Transitions play on auto-crossfades and A/B auto-pilot, timed to the beat clock.");
    ImGui::TextDisabled("Tab switches focus. Ctrl+click a preset loads the other deck. Ctrl+Left/Right nudges the fader.");
    ImGui::End();
}

void drawFx() {
    ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Effects", &S.showFx, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    if (ImGui::CollapsingHeader("Custom shaders", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("Shader", S.userShader.empty() ? "None" : S.userShader.c_str())) {
            if (ImGui::Selectable("None", S.userShader.empty())) S.userShader.clear();
            for (auto& u : userShaders)
                if (ImGui::Selectable(u.name.c_str(), S.userShader == u.name)) S.userShader = u.name;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) scanShaders();
        ImGui::SameLine();
        if (ImGui::SmallButton("Folder")) plat::openPath(userShaderDir());
        ImGui::SetNextItemWidth(220);
        ImGui::SliderFloat("Amount##us", &S.userShaderAmount, 0.f, 1.f, "%.2f");
        int si = shaderIndex(S.userShader);
        if (si >= 0 && !userShaders[si].err.empty()) ImGui::TextColored(kHot, "%s", userShaders[si].err.c_str());
        float amt = 0;
        int ai = activeShader(amt);
        if (S.userShader.empty()) ImGui::TextDisabled(ai >= 0 ? ("Auto FX is using: " + userShaders[ai].name).c_str() : "Auto FX picks shaders on drops when this is None.");
        ImGui::TextDisabled("%d shaders. Add your own .glsl files (ShaderToy style) to Documents\\Amethyst\\Shaders.", (int)userShaders.size());
    }
    FxParams& f = S.fx;
    ImGui::Checkbox("Kaleidoscope  F2", &f.kaleido); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##ks", &f.kaleidoSegments, 2, 16, "%d segments");
    ImGui::Checkbox("Mirror  F3", &f.mirror); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::Combo("##mm", &f.mirrorMode, "Off\0Horizontal\0Vertical\0Quad\0");
    ImGui::Checkbox("Feedback trails  F4", &f.feedback); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##fa", &f.feedbackAmount, 0.3f, 0.98f, "amount %.2f");
    if (f.feedback) {
        ImGui::Indent(); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##fz", &f.feedbackZoom, 0.9f, 1.15f, "trail zoom %.3f");
        ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##fr", &f.feedbackRotate, -0.1f, 0.1f, "trail rotate %.3f"); ImGui::Unindent();
    }
    ImGui::Checkbox("RGB split  F5", &f.rgbSplit); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##rs", &f.rgbSplitAmount, 0.001f, 0.05f, "%.3f");
    ImGui::Checkbox("Pixelate  F6", &f.pixelate); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##px", &f.pixelSize, 2, 64, "%.0f px");
    ImGui::Checkbox("Invert  F7", &f.invert);
    ImGui::Checkbox("Strobe  F8", &f.strobe); ImGui::SameLine(220); ImGui::Checkbox("on beat", &f.strobeOnBeat);
    ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##sr", &f.strobeRate, 1, 16, f.strobeOnBeat ? "%.0f per bar" : "%.1f Hz");
    ImGui::TextColored(kWarn, "Strobe can trigger photosensitive reactions — use with care.");
    ImGui::Separator();
    ImGui::TextDisabled("Transform");
    ImGui::SliderFloat("Zoom", &f.zoom, 0.4f, 4.f, "%.2fx", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Rotate speed", &f.rotateSpeed, -2, 2, "%.2f rad/s");
    ImGui::SliderFloat("Swirl", &f.swirl, -2, 2, "%.2f");
    ImGui::Separator();
    ImGui::TextDisabled("Color");
    ImGui::SliderFloat("Hue shift", &f.hue, 0, 1, "%.2f");
    ImGui::SliderFloat("Saturation", &f.saturation, 0, 2.5f, "%.2f");
    ImGui::SliderFloat("Contrast", &f.contrast, 0.3f, 2.5f, "%.2f");
    ImGui::SliderFloat("Brightness", &f.brightness, -0.5f, 0.5f, "%.2f");
    ImGui::Separator();
    if (ImGui::Button("Reset all effects")) actions.invoke("fx_reset", true);
    ImGui::End();
}

void drawOverlaysPanel() {
    ImGui::SetNextWindowSize(ImVec2(440, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Overlays", &S.showOverlays, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::TextDisabled("Drop an image or video file on the window to use it here.");
    if (ImGui::CollapsingHeader("Video layer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("video");
        drawLayerSource();
        if (ImGui::Checkbox("Show", &S.videoOn) && S.videoOn && !video.active() && !S.videoPath.empty()) openVideo(S.videoPath);
        ImGui::SameLine();
        if (ImGui::Button("Choose video...")) {
            auto f = plat::pickFiles(hwndOf(win), L"Choose a video", {{L"Video", L"*.mp4;*.mov;*.wmv;*.avi;*.mkv;*.m4v"}}, false);
            if (!f.empty()) openVideo(f[0]);
        }
        ImGui::SameLine();
        bool p = video.paused;
        if (ImGui::Checkbox("Pause", &p)) video.paused = p;
        ImGui::TextDisabled("%s", video.active() ? video.name().c_str() : "(none)");
        ImGui::SliderFloat("Opacity", &S.videoOpacity, 0, 1);
        ImGui::Combo("Blend", &S.videoBlend, "Normal\0Add\0Screen\0Multiply\0");
        ImGui::Combo("Fit", &S.videoFit, "Fill\0Fit\0Stretch\0");
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Logo / image", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("logo");
        ImGui::Checkbox("Show", &S.logoOn);
        ImGui::SameLine();
        if (ImGui::Button("Choose image...")) {
            auto f = plat::pickFiles(hwndOf(win), L"Choose a logo", {{L"Images", L"*.png;*.jpg;*.jpeg"}}, false);
            if (!f.empty()) { S.logoPath = f[0]; S.logoOn = true; }
        }
        ImGui::TextDisabled("%s", S.logoPath.empty() ? "(none)" : fs::u8path(S.logoPath).filename().u8string().c_str());
        ImGui::Combo("Position", &S.logoPos, "Top left\0Top right\0Bottom left\0Bottom right\0Center\0");
        ImGui::SliderFloat("Size", &S.logoScale, 0.03f, 1.0f, "%.2f of width");
        ImGui::SliderFloat("Opacity", &S.logoOpacity, 0, 1);
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("text");
        ImGui::Checkbox("Show", &S.textOn);
        static char buf[512];
        if (strcmp(buf, S.text.c_str()) != 0 && !ImGui::IsAnyItemActive()) snprintf(buf, sizeof buf, "%s", S.text.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##t", "Text / lyrics / ticker", buf, sizeof buf)) S.text = buf;
        ImGui::Combo("Position", &S.textPos, "Top\0Center\0Bottom\0");
        ImGui::SliderFloat("Size", &S.textSize, 0.02f, 0.4f, "%.2f of height");
        ImGui::SliderFloat("Scroll speed", &S.textScroll, 0, 3, S.textScroll > 0 ? "%.2f" : "static");
        float col[3] = {S.textR, S.textG, S.textB};
        if (ImGui::ColorEdit3("Color", col)) { S.textR = col[0]; S.textG = col[1]; S.textB = col[2]; }
        ImGui::SliderFloat("Opacity", &S.textOpacity, 0, 1);
        ImGui::PopID();
    }
    ImGui::End();
}

void drawOutputs() {
    ImGui::SetNextWindowSize(ImVec2(560, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Outputs & mapping", &S.showOutputs, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    int n = SDL_GetNumVideoDisplays();
    if (ImGui::Button("+ Fullscreen output")) ImGui::OpenPopup("adddisp");
    if (ImGui::BeginPopup("adddisp")) {
        for (int i = 0; i < n; i++)
            if (ImGui::MenuItem((std::to_string(i + 1) + ": " + SDL_GetDisplayName(i)).c_str())) openOutput(i);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Windowed output")) openOutput(-1);
    ImGui::SameLine();
    if (ImGui::Checkbox("Spout", &S.spout)) setSpout(S.spout);
    ImGui::SameLine();
    if (ImGui::Button(recorder.active() ? "Stop recording" : "Record MP4")) actions.invoke("record", true);
    if (ImGui::CollapsingHeader("Lighting (Art-Net / DMX)", S.dmxOn ? ImGuiTreeNodeFlags_DefaultOpen : 0)) drawLightingTab();
    if (outputs.empty()) {
        ImGui::TextWrapped("No outputs open. Add one per projector or screen. Each output can show the final mix, a single deck or the projection map, and can be corner-pinned to fit angled screens.");
        if (ImGui::Button("Projection mapping (many surfaces, a preset each)...")) S.showMapping = true;
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabBar("outs")) {
        for (int i = 0; i < (int)outputs.size(); i++) {
            Output& o = outputs[i];
            char tab[32];
            snprintf(tab, sizeof tab, "Output %d", i + 1);
            if (!ImGui::BeginTabItem(tab)) continue;
            outputSel = i;
            ImGui::SetNextItemWidth(200);
            ImGui::Combo("Shows", &o.source, kOutputSources, IM_ARRAYSIZE(kOutputSources));
            ImGui::SameLine();
            ImGui::Checkbox("Test grid", &o.grid);
            ImGui::SameLine();
            if (ImGui::Button("Reset corners")) { float d[8] = {0, 0, 1, 0, 1, 1, 0, 1}; memcpy(o.c, d, sizeof d); }
            ImGui::SameLine();
            if (ImGui::Button("Fullscreen")) toggleFullscreen(o.win);
            ImGui::SameLine();
            bool close = ImGui::Button("Close");
            // corner-pin editor
            int ow, oh;
            SDL_GetWindowSize(o.win, &ow, &oh);
            float aw = ImGui::GetContentRegionAvail().x, ah = aw * oh / (float)std::max(1, ow);
            ah = std::min(ah, ImGui::GetContentRegionAvail().y - 30);
            aw = ah * ow / (float)std::max(1, oh);
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("canvas", ImVec2(aw, ah));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + aw, p0.y + ah), IM_COL32(0, 0, 0, 200));
            ImVec2 pts[4];
            for (int k = 0; k < 4; k++) pts[k] = ImVec2(p0.x + o.c[k * 2] * aw, p0.y + o.c[k * 2 + 1] * ah);
            dl->AddImageQuad((ImTextureID)(intptr_t)outputTexture(o.source), pts[0], pts[1], pts[2], pts[3], ImVec2(0, 1), ImVec2(1, 1), ImVec2(1, 0), ImVec2(0, 0));
            dl->AddPolyline(pts, 4, ImGui::GetColorU32(kAccent), ImDrawFlags_Closed, 2);
            static int dragK = -1;
            for (int k = 0; k < 4; k++) {
                ImGui::PushID(k);
                ImGui::SetCursorScreenPos(ImVec2(pts[k].x - 9, pts[k].y - 9));
                ImGui::InvisibleButton("h", ImVec2(18, 18));
                if (ImGui::IsItemActive()) dragK = k;
                bool hot = ImGui::IsItemHovered() || dragK == k;
                dl->AddCircleFilled(pts[k], hot ? 9.f : 7.f, hot ? ImGui::GetColorU32(kAccent2) : IM_COL32(255, 255, 255, 220));
                ImGui::PopID();
            }
            if (dragK >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 m = ImGui::GetIO().MousePos;
                o.c[dragK * 2] = std::clamp((m.x - p0.x) / aw, -0.5f, 1.5f);
                o.c[dragK * 2 + 1] = std::clamp((m.y - p0.y) / ah, -0.5f, 1.5f);
            } else if (dragK >= 0) { dragK = -1; saveSettings(); }
            ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + ah + 4));
            ImGui::TextDisabled("Drag the corners to fit the projected image onto your surface.");
            ImGui::EndTabItem();
            if (close) { closeOutput(i); break; }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void drawControl() {
    ImGui::SetNextWindowSize(ImVec2(620, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Control", &S.showControl, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    if (ImGui::BeginTabBar("ctl")) {
        if (ImGui::BeginTabItem("MIDI")) {
            ImGui::Text("Devices: ");
            ImGui::SameLine();
            if (midi.devices().empty()) ImGui::TextDisabled("none found");
            for (auto& d : midi.devices()) { ImGui::SameLine(); ImGui::TextColored(kAccent2, "%s", d.c_str()); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Rescan")) midi.open();
            ImGui::TextDisabled("Last message: %s", midi.lastEvent.empty() ? "-" : midi.lastEvent.c_str());
            ImGui::TextWrapped("Click Learn, then move a knob/fader or hit a pad. Knobs and faders work best on sliders; pads on buttons.");
            if (ImGui::BeginTable("map", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableSetupColumn("Mapping", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                for (auto& a : actions.all()) {
                    ImGui::PushID(a.id.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(a.label.c_str());
                    if (a.kind == Action::Continuous) { ImGui::SameLine(); ImGui::TextDisabled("(knob)"); }
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", a.group.c_str());
                    ImGui::TableNextColumn();
                    std::string m;
                    for (auto& b : midi.bindings)
                        if (b.action == a.id) m += (m.empty() ? "" : ", ") + Midi::describe(b);
                    ImGui::TextUnformatted(midi.learning == a.id ? "move a control..." : m.empty() ? "-" : m.c_str());
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton(midi.learning == a.id ? "Cancel" : "Learn")) midi.learning = midi.learning == a.id ? "" : a.id;
                    if (!m.empty()) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Clear")) {
                            midi.bindings.erase(std::remove_if(midi.bindings.begin(), midi.bindings.end(), [&](const MidiBinding& b) { return b.action == a.id; }), midi.bindings.end());
                            saveSettings();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Phone remote")) {
            bool on = S.webOn;
            if (ImGui::Checkbox("Enable web remote", &on)) { S.webOn = on; applyNetSettings(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Port##web", &S.webPort, 0) && S.webOn) applyNetSettings();
            ImGui::TextWrapped("Open one of these addresses on a phone or tablet on the same Wi-Fi. Anyone on your network can use it while it's on; Windows may ask to allow the app through the firewall.");
            if (web.running())
                for (auto& ip : HttpServer::localAddresses()) {
                    std::string url = "http://" + ip + ":" + std::to_string(S.webPort) + "/";
                    ImGui::TextColored(kAccent2, "%s", url.c_str());
                    ImGui::SameLine();
                    ImGui::PushID(url.c_str());
                    if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(url.c_str());
                    ImGui::PopID();
                }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("DJ link", nullptr, selectDjTab ? ImGuiTabItemFlags_SetSelected : 0)) {
            selectDjTab = false;
            drawDjTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("OSC")) {
            bool on = S.oscOn;
            if (ImGui::Checkbox("Enable OSC (UDP)", &on)) { S.oscOn = on; applyNetSettings(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Port##osc", &S.oscPort, 0) && S.oscOn) applyNetSettings();
            ImGui::TextDisabled("Last: %s", osc.lastMessage.empty() ? "-" : osc.lastMessage.c_str());
            ImGui::TextWrapped("Send /amethyst/<action> with an optional float (0..1 for sliders, 1/0 for buttons). Examples: /amethyst/next, /amethyst/crossfade 0.5, /amethyst/fx_kaleido, /amethyst/cue/3, /amethyst/tap");
            if (ImGui::BeginTable("osc", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
                for (auto& a : actions.all()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(kAccent2, "/amethyst/%s", a.id.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s%s", a.label.c_str(), a.kind == Action::Continuous ? " (0..1)" : "");
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void drawSetlists() {
    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Setlists", &S.showSetlists, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputTextWithHint("##new", "New setlist name", setlistNameBuf, sizeof setlistNameBuf, ImGuiInputTextFlags_EnterReturnsTrue) && setlistNameBuf[0]) {
        setlists.push_back({setlistNameBuf, {}});
        S.activeSetlist = setlistNameBuf;
        setlistNameBuf[0] = 0;
        saveLibrary();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+L adds the current preset");
    for (size_t si = 0; si < setlists.size(); si++) {
        Setlist& s = setlists[si];
        ImGui::PushID((int)si);
        bool active = S.activeSetlist == s.name;
        bool open = ImGui::CollapsingHeader((s.name + "  (" + std::to_string(s.items.size()) + ")" + (active ? "  - active" : "") + "###sl").c_str());
        if (open) {
            if (ImGui::SmallButton("Play as pool")) { setPool("setlist:" + s.name); S.shuffle = false; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Make active")) S.activeSetlist = s.name;
            ImGui::SameLine();
            if (ImGui::SmallButton("Add current")) addToSetlist(decks[focus].cur, s.name);
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                if (S.pool == "setlist:" + s.name) setPool("all");
                setlists.erase(setlists.begin() + si);
                saveLibrary();
                ImGui::PopID();
                break;
            }
            for (size_t i = 0; i < s.items.size(); i++) {
                ImGui::PushID((int)i);
                auto it = byPath.find(s.items[i]);
                std::string nm = it == byPath.end() ? "(missing) " + s.items[i] : lib[it->second].name;
                ImGui::Selectable((std::to_string(i + 1) + ". " + nm).c_str(), false, ImGuiSelectableFlags_AllowOverlap);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && it != byPath.end()) loadPreset(focus, it->second, false);
                if (ImGui::BeginDragDropSource()) { int k = (int)i; ImGui::SetDragDropPayload("SLITEM", &k, sizeof k); ImGui::TextUnformatted(nm.c_str()); ImGui::EndDragDropSource(); }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto* pl = ImGui::AcceptDragDropPayload("SLITEM")) {
                        int from = *(const int*)pl->Data;
                        std::string item = s.items[from];
                        s.items.erase(s.items.begin() + from);
                        s.items.insert(s.items.begin() + i, item);
                        saveLibrary();
                        if (S.pool == "setlist:" + s.name) rebuildView();
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine(ImGui::GetContentRegionMax().x - 20);
                if (ImGui::SmallButton("x")) { s.items.erase(s.items.begin() + i); saveLibrary(); ImGui::PopID(); break; }
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    ImGui::End();
}

int editorResize(ImGuiInputTextCallbackData* d) {
    if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* s = (std::string*)d->UserData;
        s->resize(d->BufTextLen);
        d->Buf = s->data();
    }
    return 0;
}

void drawEditor() {
    ImGui::SetNextWindowSize(ImVec2(760, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Preset editor", &editorOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    static bool live = true;
    auto edited = [&] {
        editorDirty = true;
        if (live) editorApplyAt = now() + 0.15;  // debounce: compile once the user pauses
    };
    ImGuiIO& io = ImGui::GetIO();
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
    ImGui::BeginDisabled(!visualEditor.canUndo());
    if ((ImGui::Button("Undo") || (focused && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))) &&
        visualEditor.undo(editorText))
        edited();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!visualEditor.canRedo());
    if ((ImGui::Button("Redo") || (focused && io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))))) &&
        visualEditor.redo(editorText))
        edited();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Revert") && visualEditor.revert(editorText)) edited();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go back to the preset as it was when you opened it.");
    ImGui::SameLine();
    if (ImGui::Button("Save as...  Ctrl+S")) saveEditorAs();
    ImGui::SameLine();
    if (ImGui::Button("Edit current preset")) openEditor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load the preset now playing on the focused deck.");
    ImGui::SameLine();
    ImGui::Checkbox("Live", &live);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show every change on screen straight away.");
    if (!live) {
        ImGui::SameLine();
        if (ImGui::Button("Apply  Ctrl+Enter")) applyEditor();
    }
    ImGui::TextDisabled("%s%s", editorStatus.c_str(), editorDirty ? "  (modified)" : "");
    if (editorText.empty()) ImGui::TextWrapped("Pick a preset first, then press \"Edit current preset\".");
    if (ImGui::BeginTabBar("##edtabs")) {
        if (visualEditor.draw(editorText)) edited();
        if (ImGui::BeginTabItem("Code (advanced)")) {
            editorText.reserve(editorText.size() + 1);
            if (ImGui::InputTextMultiline("##code", editorText.data(), editorText.capacity() + 1, ImVec2(-1, -1),
                                          ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput, editorResize, &editorText))
                edited();
            if (ImGui::IsItemDeactivatedAfterEdit()) visualEditor.external(editorText);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (editorApplyAt > 0 && now() >= editorApplyAt) {
        editorApplyAt = 0;
        applyEditor();
    }
    ImGui::End();
}

void drawToasts(float top) {
    double t = now();
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(), [&](const Toast& x) { return x.until < t; }), toasts.end());
    if (toasts.empty() || !S.showToasts) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, top + 10), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::Begin("##toasts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
    for (auto& x : toasts) {
        if (x.err) ImGui::TextColored(ImVec4(1, 0.7f, 0.76f, 1), "%s", x.text.c_str());
        else ImGui::TextUnformatted(x.text.c_str());
    }
    ImGui::End();
}

void drawAudioMenu() {
    if (!audioMenuOpen) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.3f), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowSize(ImVec2(520 * io.FontGlobalScale, 0));
    ImGui::Begin("Audio input  (arrows + Enter, Esc closes)", &audioMenuOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    int i = 0;
    auto item = [&](const std::string& text, bool active) {
        ImGui::PushID(i);
        if (ImGui::Selectable((std::string(active ? "> " : "   ") + text).c_str(), audioMenuSel == i)) audioMenuActivate(i);
        ImGui::PopID();
        i++;
    };
    for (auto& d : audioDevices) {
        bool active = audio.deviceName() == d.name &&
                      ((d.loopback && audio.source() == AudioEngine::Source::Loopback) || (!d.loopback && audio.source() == AudioEngine::Source::Input));
        item((d.loopback ? "System audio:  " : "Input:  ") + d.name + (d.isDefault ? "  (default)" : ""), active);
    }
    item("Audio file...", audio.source() == AudioEngine::Source::File);
    item("No audio", audio.source() == AudioEngine::Source::None);
    ImGui::End();
}

// ----- group colours shared by the keyboard editor and help
ImU32 groupColor(const std::string& g, int alpha = 255) {
    static const std::map<std::string, ImVec4> c = {
        {"Presets", ImVec4(0.70f, 0.53f, 1.0f, 1)}, {"Cues", ImVec4(1.0f, 0.62f, 0.25f, 1)}, {"Library", ImVec4(0.95f, 0.82f, 0.35f, 1)},
        {"Mixer", ImVec4(0.31f, 0.90f, 0.78f, 1)}, {"Auto-pilot", ImVec4(0.38f, 0.72f, 1.0f, 1)}, {"Tempo", ImVec4(1.0f, 0.43f, 0.78f, 1)},
        {"Performance", ImVec4(1.0f, 0.36f, 0.48f, 1)}, {"FX", ImVec4(0.55f, 0.95f, 0.45f, 1)}, {"Overlays", ImVec4(0.85f, 0.85f, 0.95f, 1)},
        {"Output", ImVec4(0.45f, 0.85f, 1.0f, 1)}, {"Scenes", ImVec4(1.0f, 0.86f, 0.3f, 1)}, {"View", ImVec4(0.62f, 0.60f, 0.70f, 1)}, {"Audio", ImVec4(0.95f, 0.65f, 0.55f, 1)}};
    auto it = c.find(g);
    ImVec4 v = it == c.end() ? ImVec4(0.7f, 0.7f, 0.7f, 1) : it->second;
    v.w = alpha / 255.f;
    return ImGui::ColorConvertFloat4ToU32(v);
}
const char* kGroupOrder[] = {"Presets", "Cues", "Scenes", "Library", "Mixer", "Auto-pilot", "Tempo", "Performance", "FX", "Overlays", "Output", "Audio", "View"};
bool assignable(const Action& a) { return a.kind != Action::Continuous; }  // sliders belong on MIDI knobs

// ----- keyboard editor
struct KeyCap { int key; const char* label; float w; };  // key 0 = gap, -1 = not assignable
const std::vector<std::vector<KeyCap>>& keyboardRows() {
    static const std::vector<std::vector<KeyCap>> rows = {
        {{-1, "Esc", 1}, {0, nullptr, 1}, {SDLK_F1, "F1", 1}, {SDLK_F2, "F2", 1}, {SDLK_F3, "F3", 1}, {SDLK_F4, "F4", 1}, {0, nullptr, .5f},
         {SDLK_F5, "F5", 1}, {SDLK_F6, "F6", 1}, {SDLK_F7, "F7", 1}, {SDLK_F8, "F8", 1}, {0, nullptr, .5f},
         {SDLK_F9, "F9", 1}, {SDLK_F10, "F10", 1}, {SDLK_F11, "F11", 1}, {SDLK_F12, "F12", 1}},
        {{SDLK_BACKQUOTE, "`", 1}, {SDLK_1, "1", 1}, {SDLK_2, "2", 1}, {SDLK_3, "3", 1}, {SDLK_4, "4", 1}, {SDLK_5, "5", 1}, {SDLK_6, "6", 1},
         {SDLK_7, "7", 1}, {SDLK_8, "8", 1}, {SDLK_9, "9", 1}, {SDLK_0, "0", 1}, {SDLK_MINUS, "-", 1}, {SDLK_EQUALS, "=", 1}, {SDLK_BACKSPACE, "Backspace", 2}},
        {{SDLK_TAB, "Tab", 1.5f}, {SDLK_q, "Q", 1}, {SDLK_w, "W", 1}, {SDLK_e, "E", 1}, {SDLK_r, "R", 1}, {SDLK_t, "T", 1}, {SDLK_y, "Y", 1},
         {SDLK_u, "U", 1}, {SDLK_i, "I", 1}, {SDLK_o, "O", 1}, {SDLK_p, "P", 1}, {SDLK_LEFTBRACKET, "[", 1}, {SDLK_RIGHTBRACKET, "]", 1}, {SDLK_BACKSLASH, "\\", 1.5f}},
        {{-1, "Caps", 1.75f}, {SDLK_a, "A", 1}, {SDLK_s, "S", 1}, {SDLK_d, "D", 1}, {SDLK_f, "F", 1}, {SDLK_g, "G", 1}, {SDLK_h, "H", 1},
         {SDLK_j, "J", 1}, {SDLK_k, "K", 1}, {SDLK_l, "L", 1}, {SDLK_SEMICOLON, ";", 1}, {SDLK_QUOTE, "'", 1}, {SDLK_RETURN, "Enter", 2.25f}},
        {{-1, "Shift", 2.25f}, {SDLK_z, "Z", 1}, {SDLK_x, "X", 1}, {SDLK_c, "C", 1}, {SDLK_v, "V", 1}, {SDLK_b, "B", 1}, {SDLK_n, "N", 1},
         {SDLK_m, "M", 1}, {SDLK_COMMA, ",", 1}, {SDLK_PERIOD, ".", 1}, {SDLK_SLASH, "/", 1}, {-1, "Shift", 2.75f}},
        {{-1, "Ctrl", 1.5f}, {-1, "Win", 1.25f}, {-1, "Alt", 1.25f}, {SDLK_SPACE, "Space", 6.25f}, {-1, "Alt", 1.25f}, {-1, "Win", 1.25f},
         {-1, "Menu", 1.25f}, {-1, "Ctrl", 1.5f}},
    };
    return rows;
}
const std::vector<std::vector<KeyCap>>& navRows() {
    static const std::vector<std::vector<KeyCap>> rows = {
        {},
        {{SDLK_INSERT, "Ins", 1}, {SDLK_HOME, "Home", 1}, {SDLK_PAGEUP, "PgUp", 1}},
        {{SDLK_DELETE, "Del", 1}, {SDLK_END, "End", 1}, {SDLK_PAGEDOWN, "PgDn", 1}},
        {},
        {{0, nullptr, 1}, {SDLK_UP, "Up", 1}, {0, nullptr, 1}},
        {{SDLK_LEFT, "Left", 1}, {SDLK_DOWN, "Down", 1}, {SDLK_RIGHT, "Right", 1}},
    };
    return rows;
}

std::string armedAction;  // selected in the list: the next clicked key gets it
int assignKey = 0;        // key whose "assign" popup is open
char keyFilter[64] = "";

void drawKeyCap(const KeyCap& kc, ImVec2 pos, ImVec2 size, int layer) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(kc.key > 0 ? kc.key : (int)(intptr_t)kc.label + (int)pos.x);
    ImGui::InvisibleButton("cap", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImVec2 end(pos.x + size.x, pos.y + size.y);
    if (kc.key < 0) {  // modifiers & Esc
        dl->AddRectFilled(pos, end, IM_COL32(255, 255, 255, 8), 6);
        dl->AddRect(pos, end, IM_COL32(255, 255, 255, 25), 6);
        dl->AddText(ImVec2(pos.x + 6, pos.y + 4), IM_COL32(150, 148, 165, 255), kc.label);
        if (hovered) ImGui::SetTooltip("%s", strcmp(kc.label, "Esc") == 0 ? "Esc is fixed: closes windows, leaves text fields, clears search"
                                                                           : "Modifier: pick the Shift / Ctrl / Alt layer above");
        ImGui::PopID();
        return;
    }
    auto it = keymap.find({kc.key, layer});
    Action* a = it == keymap.end() ? nullptr : actions.find(it->second);
    ImU32 bg = a ? groupColor(a->group, hovered ? 120 : 80) : IM_COL32(255, 255, 255, hovered ? 28 : 14);
    dl->AddRectFilled(pos, end, bg, 6);
    dl->AddRect(pos, end, a ? groupColor(a->group, 200) : IM_COL32(255, 255, 255, 40), 6, 0, (hovered || !armedAction.empty()) ? 2.f : 1.f);
    dl->AddText(ImVec2(pos.x + 6, pos.y + 3), IM_COL32(236, 235, 245, 255), kc.label);
    if (a) {
        float fs = ImGui::GetFontSize() * 0.78f;
        ImVec4 clip(pos.x + 3, pos.y, end.x - 3, end.y - 2);
        dl->AddText(nullptr, fs, ImVec2(pos.x + 5, pos.y + ImGui::GetFontSize() + 2), IM_COL32(255, 255, 255, 235), a->label.c_str(), nullptr,
                    size.x - 8, &clip);
    }
    std::string chord = chordName(kc.key, layer);
    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        ImGui::SetTooltip("%s  ->  %s\nClick: choose action   Right-click: clear   Drag: move to another key", chord.c_str(), a ? a->label.c_str() : "(unassigned)");
    // drag a bound key onto another key to move the shortcut
    if (a && ImGui::BeginDragDropSource()) {
        int payload[2] = {kc.key, layer};
        ImGui::SetDragDropPayload("KEYBIND", payload, sizeof payload);
        ImGui::Text("%s  (%s)", a->label.c_str(), chord.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (auto* p = ImGui::AcceptDragDropPayload("ACTION")) bindKey(kc.key, layer, std::string((const char*)p->Data));
        if (auto* p = ImGui::AcceptDragDropPayload("KEYBIND")) {
            const int* src = (const int*)p->Data;
            auto s = keymap.find({src[0], src[1]});
            if (s != keymap.end() && !(src[0] == kc.key && src[1] == layer)) {
                std::string id = s->second;
                keymap.erase(s);
                bindKey(kc.key, layer, id);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left)) {
        if (!armedAction.empty()) { bindKey(kc.key, layer, armedAction); }
        else { assignKey = kc.key; ImGui::OpenPopup("##assign"); }
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && a) bindKey(kc.key, layer, "");
    if (assignKey == kc.key && ImGui::BeginPopup("##assign")) {
        ImGui::TextColored(kAccent, "%s", chord.c_str());
        static char f[64] = "";
        if (ImGui::IsWindowAppearing()) { f[0] = 0; ImGui::SetKeyboardFocusHere(); }
        ImGui::SetNextItemWidth(260);
        ImGui::InputTextWithHint("##f", "Filter actions...", f, sizeof f);
        if (a && ImGui::Selectable("(clear this key)")) bindKey(kc.key, layer, "");
        ImGui::BeginChild("##al", ImVec2(300, 320));
        std::string lf = lower(f);
        for (const char* g : kGroupOrder) {
            bool header = false;
            for (auto& act : actions.all()) {
                if (act.group != g || !assignable(act) || (!lf.empty() && lower(act.label).find(lf) == std::string::npos)) continue;
                if (!header) { ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(groupColor(g)), "%s", g); header = true; }
                if (ImGui::Selectable(("   " + act.label + "##" + act.id).c_str(), a && a->id == act.id)) {
                    bindKey(kc.key, layer, act.id);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void drawKeyboard() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x - 40, 1380.f * io.FontGlobalScale), 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    static int lastFrame = -10;
    if (ImGui::GetFrameCount() - lastFrame > 1) ImGui::SetNextWindowFocus();  // opens on top
    lastFrame = ImGui::GetFrameCount();
    if (!ImGui::Begin("Keyboard layout", &keyboardOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    static int layer = 0;
    const int layers[] = {0, 1, 2, 4, 3, 6};
    const char* layerNames[] = {"No modifier", "Shift", "Ctrl", "Alt", "Ctrl+Shift", "Ctrl+Alt"};
    for (int i = 0; i < 6; i++) {
        if (i) ImGui::SameLine();
        int count = 0;
        for (auto& [k, id] : keymap) count += k.second == layers[i];
        char lbl[48];
        snprintf(lbl, sizeof lbl, "%s (%d)", layerNames[i], count);
        if (ImGui::RadioButton(lbl, layer == layers[i])) layer = layers[i];
    }
    // holding real modifiers previews that layer
    int held = (io.KeyShift ? 1 : 0) | (io.KeyCtrl ? 2 : 0) | (io.KeyAlt ? 4 : 0);
    int shown = held && !io.WantTextInput ? held : layer;
    ImGui::SameLine(0, 30);
    if (ImGui::Button("Reset to defaults")) ImGui::OpenPopup("##reset");
    if (ImGui::BeginPopup("##reset")) {
        ImGui::Text("Replace all shortcuts with the defaults?");
        if (ImGui::Button("Reset")) { defaultKeymap(); saveSettings(); toast(false, "Shortcuts reset to defaults"); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!captureAction.empty()) {
        Action* ca = actions.find(captureAction);
        ImGui::TextColored(kWarn, "Press the key (with Shift/Ctrl/Alt if you like) for \"%s\" ...  Esc cancels", ca ? ca->label.c_str() : "?");
    } else if (!armedAction.empty()) {
        Action* aa = actions.find(armedAction);
        ImGui::TextColored(kAccent2, "Click keys to assign \"%s\" (layer: %s).  Click the action again to stop.", aa ? aa->label.c_str() : "?",
                           shown ? chordName(0, shown).substr(0, chordName(0, shown).size() - keyName(0).size()).c_str() : "none");
    } else {
        ImGui::TextDisabled("Drag an action onto a key, click a key to choose its action, or double-click an action and press the key. Hold Shift/Ctrl/Alt to see those layers.");
    }

    // left: action list
    float listW = 300 * io.FontGlobalScale;
    ImGui::BeginChild("##actions", ImVec2(listW, 0), ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##kf", "Filter actions...", keyFilter, sizeof keyFilter);
    std::string lf = lower(keyFilter);
    for (const char* g : kGroupOrder) {
        bool header = false;
        for (auto& a : actions.all()) {
            if (a.group != g || !assignable(a) || (!lf.empty() && lower(a.label).find(lf) == std::string::npos)) continue;
            if (!header) { ImGui::SeparatorText(g); header = true; }
            ImGui::PushID(a.id.c_str());
            auto chords = chordsFor(a.id);
            std::string keys;
            for (auto& c : chords) keys += (keys.empty() ? "" : "  ") + c;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + 3), ImVec2(p.x + 4, p.y + ImGui::GetTextLineHeight() - 1), groupColor(g), 2);
            ImGui::SetCursorScreenPos(ImVec2(p.x + 9, p.y));
            if (ImGui::Selectable(a.label.c_str(), armedAction == a.id || captureAction == a.id, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { captureAction = a.id; armedAction.clear(); }
                else armedAction = armedAction == a.id ? "" : a.id;
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ACTION", a.id.c_str(), a.id.size() + 1);
                ImGui::Text("%s", a.label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nDrag onto a key, click then click keys, or double-click and press a key.", keys.empty() ? "no shortcut" : keys.c_str());
            if (!keys.empty()) {
                float w = ImGui::CalcTextSize(keys.c_str()).x;
                ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8, listW - w - 24));
                ImGui::TextDisabled("%s", keys.c_str());
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    // right: the keyboard
    ImGui::SameLine();
    ImGui::BeginChild("##kb", ImVec2(0, 0));
    float avail = ImGui::GetContentRegionAvail().x;
    float unit = std::min(avail / 19.f, 70.f * io.FontGlobalScale);
    float keyH = unit * 1.05f, gap = std::max(3.f, unit * 0.07f);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    auto drawRows = [&](const std::vector<std::vector<KeyCap>>& rows, float x0) {
        for (size_t r = 0; r < rows.size(); r++) {
            float x = x0, y = origin.y + r * (keyH + gap) + (r > 0 ? keyH * 0.35f : 0);
            for (auto& kc : rows[r]) {
                float w = kc.w * unit;
                if (kc.key != 0) drawKeyCap(kc, ImVec2(x, y), ImVec2(w - gap, keyH), shown);
                x += w;
            }
        }
    };
    drawRows(keyboardRows(), origin.x);
    drawRows(navRows(), origin.x + 15.5f * unit);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 6 * (keyH + gap) + keyH * 0.5f));
    // legend
    for (const char* g : kGroupOrder) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + 3), ImVec2(p.x + 12, p.y + 15), groupColor(g), 3);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 16, p.y));
        ImGui::TextDisabled("%s", g);
        ImGui::SameLine(0, 14);
    }
    ImGui::NewLine();
    ImGui::TextDisabled("Keypad keys act like their main-keyboard twins. Sliders (crossfader, speed...) are mapped to MIDI knobs under Control > MIDI.");
    ImGui::EndChild();
    ImGui::End();
}

// ----- help, generated from the live key map
void drawHelp() {
    if (!helpOpen) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x - 40, 1240.f * io.FontGlobalScale), std::min(io.DisplaySize.y - 60, 760.f)), ImGuiCond_Appearing);
    static bool wasOpen = false;
    if (!wasOpen) ImGui::SetNextWindowFocus();  // always opens on top of the panels
    ImGui::Begin("Keyboard shortcuts", &helpOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    wasOpen = helpOpen;
    if (ImGui::Button(("Customize keys..." + keyHint("keyboard")).c_str())) { keyboardOpen = true; }
    ImGui::SameLine();
    ImGui::TextDisabled("This list always matches your current shortcuts.  Esc: close / leave text field / clear search");

    struct Row { std::string keys, label; };
    std::vector<std::pair<std::string, std::vector<Row>>> sections;
    for (const char* g : kGroupOrder) {
        std::vector<Row> rows;
        std::set<std::string> done;
        if (std::string(g) == "Cues") {  // collapse the 1..0 families when they follow the digit pattern
            for (auto fam : {std::make_pair("cue", "Fire cue"), std::make_pair("store_cue", "Store current preset in cue"), std::make_pair("clear_cue", "Clear cue")}) {
                int mods = -1;
                bool pattern = true;
                for (int i = 0; i < 10 && pattern; i++) {
                    std::string id = fam.first + std::to_string((i + 1) % 10);
                    SDL_Keycode want = i == 9 ? SDLK_0 : SDLK_1 + i;
                    bool found = false;
                    for (auto& [k, a] : keymap)
                        if (a == id && k.first == (int)want && (mods < 0 || k.second == mods)) { mods = k.second; found = true; break; }
                    pattern = found;
                }
                if (pattern) {
                    rows.push_back({chordName(SDLK_1, mods) + " ... 0", fam.second});
                    for (int i = 0; i < 10; i++) done.insert(fam.first + std::to_string(i));
                }
            }
        }
        for (auto& a : actions.all()) {
            if (a.group != g || done.count(a.id)) continue;
            auto c = chordsFor(a.id);
            if (c.empty()) continue;
            std::string keys;
            for (auto& x : c) keys += (keys.empty() ? "" : "  /  ") + x;
            rows.push_back({keys, a.label});
        }
        if (!rows.empty()) sections.push_back({g, rows});
    }
    // balance sections over 4 columns
    const int cols = 4;
    size_t total = 0;
    for (auto& s : sections) total += s.second.size() + 2;
    std::vector<std::vector<size_t>> colSecs(cols);
    size_t acc = 0;
    for (size_t i = 0; i < sections.size(); i++) {
        int c = std::min(cols - 1, (int)(acc * cols / std::max<size_t>(1, total)));
        colSecs[c].push_back(i);
        acc += sections[i].second.size() + 2;
    }
    ImGui::BeginChild("##helpbody");
    if (ImGui::BeginTable("helpcols", cols)) {
        for (int c = 0; c < cols; c++) {
            ImGui::TableNextColumn();
            for (size_t si : colSecs[c]) {
                auto& sec = sections[si];
                std::string title = sec.first;
                for (auto& ch : title) ch = (char)toupper((unsigned char)ch);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(groupColor(sec.first)), "%s", title.c_str());
                if (ImGui::BeginTable(sec.first.c_str(), 2, ImGuiTableFlags_SizingFixedFit)) {
                    for (auto& r : sec.second) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(r.keys.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", r.label.c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Every action can also be mapped to MIDI (Control > MIDI), driven by OSC, or used from the phone remote.");
    ImGui::EndChild();
    ImGui::End();
}


void drawScenes() {
    ImGui::SetNextWindowSize(ImVec2(620, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scenes & show", &S.showScenes, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::TextDisabled("A scene is a whole look: both deck presets, crossfader, mix mode, effects and overlays.");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Morph time", &S.sceneMorphBeats, 0, 16, S.sceneMorphBeats < 0.01f ? "instant" : "%.1f beats");
    ImGui::SameLine();
    ImGui::TextDisabled("Click: recall   Shift+click: store   Right-click: rename / clear");
    float w = (ImGui::GetContentRegionAvail().x - 3 * 8) / 4;
    for (int i = 0; i < kScenes; i++) {
        Scene& s = scenes[i];
        ImGui::PushID(i);
        if (i % 4) ImGui::SameLine(0, 8);
        bool active = i == activeScene;
        ImGui::PushStyleColor(ImGuiCol_Button, s.used ? (active ? ImVec4(0.95f, 0.82f, 0.35f, 0.45f) : ImVec4(0.95f, 0.82f, 0.35f, 0.16f)) : ImVec4(1, 1, 1, 0.04f));
        std::string label = (s.used ? s.name : "empty") + "\n" + (i < 8 ? keyHint("scene" + std::to_string(i + 1)) : "") + "##s";
        if (ImGui::Button(label.c_str(), ImVec2(w, 54))) {
            if (ImGui::GetIO().KeyShift) storeScene(i); else recallScene(i);
        }
        ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem()) {
            static char nm[64];
            if (ImGui::IsWindowAppearing()) snprintf(nm, sizeof nm, "%s", s.name.c_str());
            if (ImGui::MenuItem("Store current look here")) storeScene(i);
            ImGui::SetNextItemWidth(180);
            if (s.used && ImGui::InputText("Name", nm, sizeof nm, ImGuiInputTextFlags_EnterReturnsTrue)) { s.name = nm; saveScenes(); }
            if (s.used && ImGui::MenuItem("Clear")) { s = Scene{}; saveScenes(); }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextColored(kAccent, "Show timeline");
    ImGui::SameLine();
    if (ImGui::Button(tlPlaying ? "Stop" : "Play")) timelinePlay(!tlPlaying);
    ImGui::SameLine();
    if (ImGui::Checkbox("Loop", &tlLoop)) saveScenes();
    ImGui::SameLine();
    if (ImGui::Button("+ Step")) { timeline.push_back({std::max(0, activeScene), 4}); saveScenes(); }
    if (tlPlaying && !timeline.empty()) {
        double into = beatNow() - tlStepStart, len = timeline[tlIndex].bars * 4.0;
        ImGui::SameLine();
        ImGui::ProgressBar((float)std::clamp(into / len, 0.0, 1.0), ImVec2(-1, 0), ("step " + std::to_string(tlIndex + 1) + "  bar " + std::to_string((int)(into / 4) + 1)).c_str());
    }
    for (size_t i = 0; i < timeline.size(); i++) {
        ImGui::PushID((int)(1000 + i));
        TlStep& st = timeline[i];
        bool cur = tlPlaying && (int)i == tlIndex;
        ImGui::TextColored(cur ? kAccent2 : kDim, "%2d.", (int)i + 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("##sc", scenes[st.scene].used ? scenes[st.scene].name.c_str() : ("Scene " + std::to_string(st.scene + 1) + " (empty)").c_str())) {
            for (int k = 0; k < kScenes; k++)
                if (ImGui::Selectable(scenes[k].used ? scenes[k].name.c_str() : ("Scene " + std::to_string(k + 1) + " (empty)").c_str(), st.scene == k)) { st.scene = k; saveScenes(); }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputInt("bars", &st.bars)) { st.bars = std::clamp(st.bars, 1, 256); saveScenes(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("^") && i > 0) { std::swap(timeline[i], timeline[i - 1]); saveScenes(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { timeline.erase(timeline.begin() + i); saveScenes(); ImGui::PopID(); break; }
        ImGui::PopID();
    }
    if (timeline.empty()) ImGui::TextDisabled("Add steps to play scenes in order, each for a number of bars, locked to the beat clock.");
    ImGui::End();
}

void drawModulation() {
    ImGui::SetNextWindowSize(ImVec2(640, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Modulation", &S.showMod, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    double beat = beatNow();
    float meters[5] = {bandBass, bandMid, bandTreb, bandLevel, modSource(SrcBeat, beat)};
    for (int i = 0; i < 5; i++) {
        if (i) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::ProgressBar(std::clamp(meters[i], 0.f, 1.f), ImVec2(110, 8), "");
        ImGui::TextDisabled("%s", kModSrcNames[i]);
        ImGui::EndGroup();
    }
    ImGui::TextDisabled("Route audio bands, the beat or tempo-synced LFOs to any effect. Amount can be negative.");
    if (ImGui::Button("+ Add route")) { mods.push_back({}); saveScenes(); }
    ImGui::SameLine();
    if (ImGui::Button("Kick -> zoom pulse")) { ModRoute r; r.src = SrcBass; r.dst = DstZoom; r.amount = 0.25f; r.smooth = 0.3f; mods.push_back(r); saveScenes(); }
    ImGui::SameLine();
    if (ImGui::Button("Hats -> RGB split")) { ModRoute r; r.src = SrcTreble; r.dst = DstRgb; r.amount = 0.6f; r.smooth = 0.2f; mods.push_back(r); saveScenes(); }
    ImGui::SameLine();
    if (ImGui::Button("Slow hue drift")) { ModRoute r; r.src = SrcLfoSaw; r.dst = DstHue; r.amount = 1; r.rate = 64; r.smooth = 0; mods.push_back(r); saveScenes(); }
    bool changed = false;
    if (ImGui::BeginTable("mods", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Target");
        ImGui::TableSetupColumn("Amount");
        ImGui::TableSetupColumn("Rate / smooth");
        ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < mods.size(); i++) {
            ModRoute& r = mods[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); changed |= ImGui::Checkbox("##on", &r.on);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); changed |= ImGui::Combo("##src", &r.src, kModSrcNames, SrcCount);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); changed |= ImGui::Combo("##dst", &r.dst, kModDstNames, DstCount);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); changed |= ImGui::SliderFloat("##amt", &r.amount, -1, 1, "%.2f");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
            if (isLfo(r.src)) changed |= ImGui::SliderFloat("##rate", &r.rate, 0.25f, 64, "%.2g beats", ImGuiSliderFlags_Logarithmic);
            else changed |= ImGui::SliderFloat("##sm", &r.smooth, 0, 0.95f, "smooth %.2f");
            ImGui::TableNextColumn(); ImGui::ProgressBar(std::clamp(r.state, 0.f, 1.f), ImVec2(-1, 8), "");
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) { mods.erase(mods.begin() + i); saveScenes(); ImGui::PopID(); break; }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    static bool pending = false;
    pending |= changed;
    if (pending && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) { saveScenes(); pending = false; }
    ImGui::End();
}

void drawDjTab() {
    bool on = S.djOn;
    if (ImGui::Checkbox("Enable DJ link (Denon Engine OS / StageLinQ)", &on)) setDjLink(on);
    ImGui::TextWrapped("Connect the PC to the same network as your Engine OS players or mixer (SC LIVE, SC6000, Prime...). "
                       "Amethyst then follows the on-air deck's tempo and beat, and sees track changes.");
    if (!S.djOn) return;
    ImGui::Separator();
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImGui::TextColored(djSnap.connected ? kAccent2 : kWarn, "%s", djSnap.status.empty() ? "starting..." : djSnap.status.c_str());
    if (!djSnap.device.empty()) ImGui::TextDisabled("Device: %s", djSnap.device.c_str());
    int air = StageLinq::onAirDeck(djSnap);
    if (!djSnap.decks.empty() && ImGui::BeginTable("decks", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Deck", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Track");
        ImGui::TableSetupColumn("BPM", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Beat", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Fader", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < djSnap.decks.size(); i++) {
            const DjDeck& d = djSnap.decks[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored((int)i == air ? kAccent2 : kDim, "%d", (int)i + 1);
            ImGui::TableNextColumn(); ImGui::TextUnformatted((d.artist.empty() ? d.title : d.artist + " - " + d.title).c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.1f", d.bpm);
            ImGui::TableNextColumn(); ImGui::Text("%d.%d", (int)std::floor(d.beat / 4) + 1, ((int)std::floor(d.beat) % 4 + 4) % 4 + 1);
            ImGui::TableNextColumn(); ImGui::Text("%.0f%%", d.fader * 100);
            ImGui::TableNextColumn(); ImGui::TextDisabled("%s%s", d.playing ? "playing" : "", (int)i == air ? " ON AIR" : "");
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Checkbox("Use the on-air deck as the tempo/beat clock", &S.djTempo);
    ImGui::SetNextItemWidth(220);
    ImGui::Combo("On track change", &S.djTrackAction, "Do nothing\0Next preset\0Random preset\0Next scene\0");
    ImGui::Checkbox("Show \"Artist - Title\" on screen", &S.djShowTitle);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("##secs", &S.djTitleSecs, 2, 20, "for %.0f s");
    ImGui::TextDisabled("Serato, rekordbox and Traktor: use Ableton Link instead (Control > Tempo source).");
}

void drawLightingTab() {
    ImGui::Checkbox("Send Art-Net (DMX over network)", &S.dmxOn);
    ImGui::TextWrapped("Each fixture takes the colour of one vertical slice of the output, left to right, so lights follow the visuals.");
    static char ip[64];
    if (ImGui::IsWindowAppearing() || strcmp(ip, S.dmxIp.c_str()) != 0) {
        if (!ImGui::IsAnyItemActive()) snprintf(ip, sizeof ip, "%s", S.dmxIp.c_str());
    }
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("Target IP (255.255.255.255 = broadcast)", ip, sizeof ip)) S.dmxIp = ip;
    ImGui::SetNextItemWidth(200); ImGui::InputInt("Universe", &S.dmxUniverse); S.dmxUniverse = std::clamp(S.dmxUniverse, 0, 32767);
    ImGui::SetNextItemWidth(200); ImGui::SliderInt("Fixtures", &S.dmxFixtures, 1, 64);
    ImGui::SetNextItemWidth(200); ImGui::InputInt("Start address", &S.dmxStart); S.dmxStart = std::clamp(S.dmxStart, 1, 512);
    ImGui::SetNextItemWidth(260); ImGui::Combo("Channel layout", &S.dmxMode, kDmxModes, 4);
    ImGui::SetNextItemWidth(200); ImGui::SliderFloat("Master level", &S.dmxLevel, 0, 1);
    int last = S.dmxStart - 1 + S.dmxFixtures * kDmxWidth[std::clamp(S.dmxMode, 0, 3)];
    ImGui::TextDisabled("Uses channels %d-%d.  Packets sent: %d", S.dmxStart, std::min(512, last), artnet.sent);
    if (last > 512) ImGui::TextColored(kWarn, "Too many fixtures for one universe - extra fixtures are skipped.");
    ImVec2 p = ImGui::GetCursorScreenPos();
    float cw = std::min(28.f, (ImGui::GetContentRegionAvail().x - 4) / std::max(1, (int)dmxColors.size()));
    for (size_t i = 0; i < dmxColors.size(); i++)
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x + i * cw, p.y), ImVec2(p.x + (i + 1) * cw - 2, p.y + 24),
                                                  IM_COL32(dmxColors[i][0], dmxColors[i][1], dmxColors[i][2], 255), 3);
    ImGui::Dummy(ImVec2(1, 28));
}

void drawLayerSource() {
    ImGui::TextDisabled("Source:");
    ImGui::SameLine();
    int src = S.layerSource;
    ImGui::RadioButton("Video file", &src, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Camera / capture", &src, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Spout input", &src, 2);
    if (src != S.layerSource) { S.layerSource = src; openLayerSource(); }
    if (S.layerSource == 1) {
        static std::vector<std::string> cams;
        static double lastScan = -10;
        if (now() - lastScan > 5) { cams = VideoPlayer::cameras(); lastScan = now(); }
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Camera", S.cameraIndex < (int)cams.size() ? cams[S.cameraIndex].c_str() : "(none found)")) {
            for (int i = 0; i < (int)cams.size(); i++)
                if (ImGui::Selectable(cams[i].c_str(), S.cameraIndex == i)) { S.cameraIndex = i; openLayerSource(); }
            ImGui::EndCombo();
        }
        if (!video.active() && ImGui::Button("Open camera")) openLayerSource();
    } else if (S.layerSource == 2) {
        auto senders = spoutSenders();
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Spout sender", S.spoutInput.empty() ? "(active sender)" : S.spoutInput.c_str())) {
            if (ImGui::Selectable("(active sender)", S.spoutInput.empty())) S.spoutInput.clear();
            for (auto& s : senders)
                if (ImGui::Selectable(s.c_str(), S.spoutInput == s)) S.spoutInput = s;
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("%s", spoutRxW > 0 ? ("Receiving " + spoutRxConnected + " " + std::to_string(spoutRxW) + "x" + std::to_string(spoutRxH)).c_str()
                                               : (senders.empty() ? "No Spout senders running" : "Waiting for frames..."));
    }
}

void drawUi() {
    ImGuiIO& io = ImGui::GetIO();
    wantThumbs.clear();
    if (defocusSearch) { ImGui::ClearActiveID(); defocusSearch = false; searchHasFocus = false; }
    if (!S.showBrowser || S.uiHidden) searchHasFocus = false;
    if (!S.uiHidden) {
        float top = 0;
        if (S.showMenu) { drawMenuBar(); top = ImGui::GetFrameHeight(); }
        float hudH = 122 * io.FontGlobalScale, cueH = 64 * io.FontGlobalScale;
        float bottom = io.DisplaySize.y - 12;
        float hudY = bottom - hudH;
        float cueY = (S.showHud ? hudY : bottom) - cueH - 8;
        if (S.showHud) drawHud(hudY, hudH);
        if (S.showCues) drawCues(cueY, cueH);
        if (S.showBrowser) drawBrowser(top + 12, (S.showCues ? cueY : S.showHud ? hudY : bottom) - 8);
        if (S.showMixer) drawMixer();
        if (S.showFx) drawFx();
        if (S.showOverlays) drawOverlaysPanel();
        if (S.showOutputs) drawOutputs();
        if (S.showControl) drawControl();
        if (S.showSetlists) drawSetlists();
        if (editorOpen) drawEditor();
        drawRenderWindow();
        if (keyboardOpen) drawKeyboard();
        if (S.showScenes) drawScenes();
        if (S.showMod) drawModulation();
        if (S.showMapping) drawMapping();
        drawToasts(top);
    }
    drawHelp();
    drawAudioMenu();
    thumbs.setWanted((S.livePreviews && !S.uiHidden) ? wantThumbs : std::vector<ThumbRenderer::Want>{});
}

