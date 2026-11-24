// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== library
std::vector<std::string> texturePaths() {
    std::vector<std::string> out;
    auto add = [&](const fs::path& p) {
        std::error_code ec;
        if (fs::is_directory(p, ec)) out.push_back(p.u8string());
    };
    add(fs::u8path(plat::exeDir()) / "textures");
    add(fs::u8path(plat::documentsDir()) / "Amethyst" / "Textures");
    for (auto& d : S.folders) {
        add(fs::u8path(d));
        add(fs::u8path(d) / "textures");
        add(fs::u8path(d) / "Textures");
    }
    return out;
}

void applyTexturePaths() {
    auto paths = texturePaths();
    std::vector<const char*> c;
    for (auto& p : paths) c.push_back(p.c_str());
    for (auto& d : decks)
        for (auto& p : d.players) p.setTexturePaths(paths);
    thumbs.setTexturePaths(paths);
}

const Setlist* poolSetlist() {
    if (S.pool.rfind("setlist:", 0) != 0) return nullptr;
    std::string n = S.pool.substr(8);
    for (auto& s : setlists)
        if (s.name == n) return &s;
    return nullptr;
}

std::string poolLabel() {
    if (S.pool == "all") return "All presets";
    if (S.pool == "fav") return "Favorites";
    if (S.pool.rfind("rating:", 0) == 0) return std::string(S.pool.substr(7)) + "+ stars";
    if (S.pool.rfind("tag:", 0) == 0) return "Tag: " + S.pool.substr(4);
    if (S.pool.rfind("setlist:", 0) == 0) return "Setlist: " + S.pool.substr(8);
    if (S.pool.rfind("color:", 0) == 0) return "Colour: " + S.pool.substr(6);
    if (S.pool == "energy:calm") return "Calm";
    if (S.pool == "energy:intense") return "Intense";
    if (S.pool == "light:dark") return "Dark";
    if (S.pool == "light:bright") return "Bright";
    if (S.pool.rfind("similar:", 0) == 0) return "Similar to " + fs::u8path(S.pool.substr(8)).stem().u8string();
    return S.pool;
}

bool smartPoolMatch(int i);
void sortSimilar(std::vector<int>& v);
void queueAnalysis();
bool inPool(int i) {
    const Preset& p = lib[i];
    if (S.hideBroken && p.broken) return false;
    if (S.pool.rfind("color:", 0) == 0 || S.pool.rfind("energy:", 0) == 0 || S.pool.rfind("light:", 0) == 0) {
        if (sourceFilter >= 0 && (sourceFilter >= (int)sourceNames.size() || p.source != sourceNames[sourceFilter])) return false;
        return smartPoolMatch(i);
    }
    if (sourceFilter >= 0 && (sourceFilter >= (int)sourceNames.size() || p.source != sourceNames[sourceFilter])) return false;
    if (S.pool == "all" || S.pool.rfind("setlist:", 0) == 0) return true;
    auto it = meta.find(p.path);
    if (S.pool == "fav") return it != meta.end() && it->second.fav;
    if (S.pool.rfind("rating:", 0) == 0) return it != meta.end() && it->second.rating >= std::atoi(S.pool.c_str() + 7);
    if (S.pool.rfind("tag:", 0) == 0) {
        if (it == meta.end()) return false;
        auto& t = it->second.tags;
        return std::find(t.begin(), t.end(), S.pool.substr(4)) != t.end();
    }
    return true;
}

void rebuildAutoView();
void rebuildView() {
    std::vector<std::string> terms;
    std::istringstream ss(lower(query));
    for (std::string t; ss >> t;) terms.push_back(t);
    auto match = [&](int i) {
        if (!inPool(i)) return false;
        for (auto& t : terms)
            if (lib[i].lname.find(t) == std::string::npos) return false;
        return true;
    };
    view.clear();
    if (const Setlist* sl = poolSetlist()) {  // setlists keep their own order
        for (auto& path : sl->items) {
            auto it = byPath.find(path);
            if (it != byPath.end() && match(it->second)) view.push_back(it->second);
        }
    } else {
        for (int i = 0; i < (int)lib.size(); i++)
            if (match(i)) view.push_back(i);
        if (S.pool.rfind("similar:", 0) == 0) sortSimilar(view);
    }
    upcoming = -1;  // picked from the old filters: auto-pilot must stay inside the new ones
    rebuildAutoView();
    int cur = decks[focus].cur;
    auto it = std::find(view.begin(), view.end(), cur);
    sel = it == view.end() ? 0 : (int)(it - view.begin());
    scrollToSel = true;
}

void setPool(const std::string& p) {
    S.pool = p;
    rebuildView();
    toast(false, "Pool: %s (%d)", poolLabel().c_str(), (int)view.size());
}

void rescan() {
    std::string curPath[2];
    for (int d = 0; d < 2; d++) curPath[d] = decks[d].cur >= 0 ? lib[decks[d].cur].path : (d ? S.lastB : S.lastA);
    lib.clear();
    byPath.clear();
    sourceNames.clear();
    std::set<std::string> sources;
    auto addOne = [&](const fs::path& p, const std::string& source) {
        auto u = p.u8string();
        if (byPath.count(u)) return;
        Preset pr;
        pr.path = u;
        pr.name = p.stem().u8string();
        pr.lname = lower(pr.name);
        pr.source = source;
        pr.broken = brokenPaths.count(u) > 0;
        byPath[u] = (int)lib.size();
        lib.push_back(std::move(pr));
        sources.insert(source);
    };
    for (auto& d : S.folders) {
        fs::path root = fs::u8path(d);
        std::string label = root.filename().u8string();
        if (label.empty()) label = d;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code e2;
            if (!it->is_regular_file(e2) || !isPresetFile(it->path())) continue;
            // label = up to two sub-folder levels (e.g. "Cream of the Crop / Fractal"), else the folder name
            fs::path relDir = it->path().parent_path().lexically_relative(root);
            std::string src;
            int depth = 0;
            for (auto& part : relDir) {
                std::string s = part.u8string();
                if (s.empty() || s == ".") continue;
                src += (src.empty() ? "" : " / ") + s;
                if (++depth == 2) break;
            }
            addOne(it->path(), src.empty() ? label : src);
        }
    }
    for (auto& f : S.files) {
        std::error_code ec;
        if (fs::exists(fs::u8path(f), ec)) addOne(fs::u8path(f), "Single files");
    }
    sourceNames.assign(sources.begin(), sources.end());
    std::sort(lib.begin(), lib.end(), [](const Preset& a, const Preset& b) { return a.lname < b.lname; });
    byPath.clear();
    for (int i = 0; i < (int)lib.size(); i++) byPath[lib[i].path] = i;
    for (int d = 0; d < 2; d++) {
        auto it = byPath.find(curPath[d]);
        decks[d].cur = it == byPath.end() ? -1 : it->second;
    }
    if (sourceFilter >= (int)sourceNames.size()) sourceFilter = -1;
    upcoming = -1;
    rebuildView();
    applyTexturePaths();
    queueAnalysis();
}

void addFolder(const std::string& d) {
    if (d.empty()) return;
    if (std::find(S.folders.begin(), S.folders.end(), d) == S.folders.end()) S.folders.push_back(d);
    size_t before = lib.size();
    rescan();
    saveSettings();
    toast(false, "Added folder — %d presets found", (int)(lib.size() - before));
}

void markBroken(int idx, const std::string& why) {
    if (idx < 0 || idx >= (int)lib.size()) return;
    lib[idx].broken = true;
    brokenPaths.insert(lib[idx].path);
    fprintf(stderr, "[Amethyst] preset failed: %s (%s)\n", lib[idx].path.c_str(), why.c_str());
}

Meta& metaOf(int idx) { return meta[lib[idx].path]; }

// ===================================================================== decks & navigation
// Auto-pilot picks from the browser's filtered list, narrowed to the "stay in folder" lock. If the
// filters and the folder have nothing in common, the folder wins.
bool autoPicking = false;
std::vector<int> autoView;
bool inAutoFolder(int i) {
    if (S.autoFolder.empty()) return true;
    const std::string& p = lib[i].path;
    return p.size() > S.autoFolder.size() && p.compare(0, S.autoFolder.size(), S.autoFolder) == 0 &&
           (p[S.autoFolder.size()] == '\\' || p[S.autoFolder.size()] == '/');
}
void rebuildAutoView() {
    autoView.clear();
    if (S.autoFolder.empty()) return;
    for (int i : view)
        if (inAutoFolder(i)) autoView.push_back(i);
    if (autoView.empty())
        for (int i = 0; i < (int)lib.size(); i++)
            if (inAutoFolder(i)) autoView.push_back(i);
}
const std::vector<int>& pickPool() { return autoPicking && !S.autoFolder.empty() && !autoView.empty() ? autoView : view; }
struct AutoPickScope {  // marks a pick as made by auto-pilot
    bool old;
    AutoPickScope(bool on = true) : old(autoPicking) { autoPicking = autoPicking || on; }
    ~AutoPickScope() { autoPicking = old; }
};

int stepFrom(int base, int dir) {
    const auto& view = pickPool();
    if (view.empty()) return -1;
    auto it = std::find(view.begin(), view.end(), base);
    int i = it == view.end() ? (dir > 0 ? -1 : (int)view.size()) : (int)(it - view.begin());
    for (size_t n = 0; n < view.size(); n++) {
        i = (i + dir + (int)view.size()) % (int)view.size();
        if (!lib[view[i]].broken) return view[i];
    }
    return -1;
}

int pickRandom(int exclude) {
    const auto& view = pickPool();
    if (view.empty()) return -1;
    std::uniform_int_distribution<size_t> d(0, view.size() - 1);
    for (int n = 0; n < 24; n++) {
        int i = view[d(rng)];
        if (!lib[i].broken && i != exclude && i != decks[0].cur && i != decks[1].cur) return i;
    }
    return stepFrom(exclude, 1);
}

void sessionMark(const std::string& loading);
// Queue a preset on the deck's spare player. Never blocks: the switch happens in commitLoads()
// once the preset has compiled and rendered its first frames.
bool loadPreset(int d, int idx, bool hard, bool record = true) {
    Deck& dk = decks[d];
    if (idx < 0 || idx >= (int)lib.size()) return false;
    if (dk.blending) { dk.blending = false; dk.blendManual = -1; }  // the spare is about to be reused: finish the running blend
    dk.pendingIdx = idx;
    dk.pendingToken = ++loadTokens;
    dk.pendingHard = hard;
    dk.pendingRecord = record;
    dk.pendingAdvance = false;
    dk.pendingEditor = false;
    dk.pendingHold = false;
    dk.heldIdx = -1;
    dk.pendingBlendSecs = -1;
    dk.spare().load(lib[idx].path, dk.pendingToken);
    sessionMark(lib[idx].path);
    if (d == focus) {
        auto it = std::find(view.begin(), view.end(), idx);
        if (it != view.end()) { sel = (int)(it - view.begin()); scrollToSel = true; }
    }
    return true;
}

int navBase(int d) { return decks[d].pendingIdx >= 0 ? decks[d].pendingIdx : decks[d].cur; }

void go(int d, int dir, bool hard, bool random = false) {
    if (view.empty()) { toast(true, query.empty() ? "No presets in this pool — add a folder (Ctrl+Shift+O)" : "No presets match the search"); return; }
    int target;
    if (random) {
        const auto& pool = pickPool();
        bool upOk = upcoming >= 0 && upcoming != decks[d].cur && !lib[upcoming].broken && std::find(pool.begin(), pool.end(), upcoming) != pool.end();
        target = upOk ? upcoming : pickRandom(navBase(d));
        upcoming = -1;
    } else {
        target = stepFrom(navBase(d), dir);
    }
    if (target < 0) return;
    loadPreset(d, target, hard);
    decks[d].pendingAdvance = true;  // if it fails to load, try the next one
    decks[d].pendingDir = dir;
    decks[d].pendingRandom = random;
    decks[d].pendingAuto = autoPicking;
}

void sessionMark(const std::string& loading);
// Called every frame: finish loads that the player threads completed.
void commitLoads() {
    static bool wasLoading = false;
    bool loadingNow = decks[0].pendingIdx >= 0 || decks[1].pendingIdx >= 0;
    if (wasLoading && !loadingNow) sessionMark("");
    wasLoading = loadingNow;
    for (int d = 0; d < 2; d++) {
        Deck& dk = decks[d];
        PresetPlayer::LoadResult r;
        while (dk.spare().takeResult(r)) {
            if (r.token != dk.pendingToken) continue;  // superseded by a newer request
            int idx = dk.pendingIdx;
            dk.pendingIdx = -1;
            dk.pendingToken = 0;
            if (dk.pendingEditor) {
                editorStatus = r.ok ? "Applied to deck " + std::string(dk.name) : "Error: " + r.error;
                if (r.ok) { dk.active = 1 - dk.active; dk.blending = false; }
                continue;
            }
            if (!r.ok) {
                if (idx >= 0) {
                    markBroken(idx, r.error);
                    toast(true, "Skipped \"%s\" — %s", lib[idx].name.c_str(), r.error.c_str());
                }
                if (dk.pendingAdvance && ++dk.failStreak < 12) {
                    AutoPickScope scope(dk.pendingAuto);
                    go(d, dk.pendingDir, dk.pendingHard, dk.pendingRandom);
                }
                continue;
            }
            dk.failStreak = 0;
            if (dk.pendingHold) {
                dk.pendingHold = false;
                dk.heldIdx = idx;
                continue;
            }
            if (dk.pendingRecord && dk.cur >= 0 && dk.cur != idx) {
                dk.hist.push_back(lib[dk.cur].path);
                if (dk.hist.size() > 300) dk.hist.erase(dk.hist.begin());
            }
            dk.cur = idx;
            dk.lastSwitch = now();
            if (idx >= 0) (d ? S.lastB : S.lastA) = lib[idx].path;
            dk.active = 1 - dk.active;  // the freshly loaded player becomes visible
            double bd = dk.pendingBlendSecs >= 0 ? dk.pendingBlendSecs : S.blend;
            dk.blending = !dk.pendingHard && bd > 0.01;
            dk.blendStart = now();
            dk.blendDur = bd;
            dk.blendMode = MixCrossfade;
            if (xfadeOnLoad == d) {
                startXfade(d ? 1.f : 0.f, xfadeOnLoadSecs);
                xfadeOnLoad = -1;
            }
        }
    }
}

bool anyLoading() { return decks[0].pendingIdx >= 0 || decks[1].pendingIdx >= 0; }

void back(int d, bool hard) {
    Deck& dk = decks[d];
    while (!dk.hist.empty()) {
        auto it = byPath.find(dk.hist.back());
        dk.hist.pop_back();
        if (it != byPath.end() && !lib[it->second].broken && it->second != dk.cur) {
            if (loadPreset(d, it->second, hard, false)) return;
        }
    }
    go(d, -1, hard);
}

double beatNow() { return tempo.beatAt(now()); }

// Launch now, or at the next beat/bar boundary when quantizing.
void requestLaunch(int d, int idx, int dir, bool hard, bool random, bool fromAuto) {
    AutoPickScope scope(fromAuto);
    int q = S.quantize;
    bool quantized = q > 0 && (fromAuto || S.quantizeManual) && !hard;
    if (!quantized) {
        if (idx >= 0) loadPreset(d, idx, hard);
        else if (random) go(d, 1, hard, true);
        else if (dir < 0) back(d, hard);
        else go(d, 1, hard);
        return;
    }
    double unit = kQuantizeBeats[q];
    double b = beatNow();
    launch = {true, d, idx, hard, random, fromAuto, std::floor(b / unit + 1e-6) * unit + unit};
    if (idx < 0 && !random) launch.idx = dir < 0 ? -2 : stepFrom(decks[d].cur, 1);
}

void startXfade(float to, double dur) {
    xanim = {true, S.xfade, to, now(), std::max(0.05, dur)};
}

void autoAdvance() {
    AutoPickScope scope;
    if (S.autoDecks) {
        int off = S.xfade < 0.5f ? 1 : 0;
        int target = S.shuffle ? pickRandom(decks[off].cur) : stepFrom(decks[1 - off].cur, 1);
        if (target >= 0 && loadPreset(off, target, true)) {
            xfadeOnLoad = off;
            xfadeOnLoadSecs = transitionSeconds();
        }
    } else {
        int on = S.xfade < 0.5f ? 0 : 1;
        requestLaunch(on, -1, 1, false, S.shuffle, false);
    }
}

void fireCue(int slot) {
    int label = (slot + 1) % 10;
    if (S.cues[slot].empty()) { toast(false, "Cue %d is empty — Shift+%d stores the current preset", label, label); return; }
    auto it = byPath.find(S.cues[slot]);
    if (it == byPath.end()) { toast(true, "Cue %d: preset is no longer in the library", label); return; }
    requestLaunch(focus, it->second, 1, false, false, false);
}
void storeCue(int slot) {
    int cur = decks[focus].cur;
    if (cur < 0) return;
    S.cues[slot] = lib[cur].path;
    saveSettings();
    toast(false, "Stored in cue %d: %s", (slot + 1) % 10, lib[cur].name.c_str());
}
void clearCue(int slot) {
    S.cues[slot].clear();
    saveSettings();
    toast(false, "Cleared cue %d", (slot + 1) % 10);
}

void toggleFavorite(int idx) {
    if (idx < 0) return;
    Meta& m = metaOf(idx);
    m.fav = !m.fav;
    saveLibrary();
    toast(false, m.fav ? "Favorited: %s" : "Unfavorited: %s", lib[idx].name.c_str());
    if (S.pool == "fav") rebuildView();
}
void setRating(int idx, int r) {
    if (idx < 0) return;
    metaOf(idx).rating = std::clamp(r, 0, 5);
    saveLibrary();
    toast(false, "%s: %d star%s", lib[idx].name.c_str(), r, r == 1 ? "" : "s");
    if (S.pool.rfind("rating:", 0) == 0) rebuildView();
}
void addToSetlist(int idx, const std::string& name) {
    if (idx < 0) return;
    for (auto& s : setlists)
        if (s.name == name) {
            s.items.push_back(lib[idx].path);
            saveLibrary();
            toast(false, "Added to setlist \"%s\" (%d)", name.c_str(), (int)s.items.size());
            if (S.pool == "setlist:" + name) rebuildView();
            return;
        }
    setlists.push_back({name, {lib[idx].path}});
    S.activeSetlist = name;
    saveLibrary();
    toast(false, "Created setlist \"%s\"", name.c_str());
}

