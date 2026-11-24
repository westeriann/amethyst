// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ===================================================================== projection mapping
// Any number of surfaces (rectangles with perspective, free shapes, circles) are drawn in list order onto
// one "map" picture that outputs can show. Each surface shows its own preset (its own projectM player),
// the main output, a deck, a colour, or the same picture as another surface. Surfaces can pulse with bass,
// hats or snares, and auto-pilot changes their presets on drops, breakdowns, new songs and phrases.

enum MapSource { MapOwn, MapMain, MapDeckA, MapDeckB, MapColour, MapSame, MapSourceCount };
const char* kMapSourceNames[] = {"Its own preset", "Main output", "Deck A", "Deck B", "Solid colour", "Same picture as..."};
const char* kMapReactNames[] = {"Nothing", "Bass", "Hi-hats", "Snares / claps", "Beat"};
const int kMaxMapPlayers = 16;

struct Surface {
    std::string name = "Surface";
    std::vector<float> pts;  // x,y pairs in 0..1 of the map, top-left origin
    int source = MapOwn;
    std::string preset, folder, link;  // own preset path; folder to pick from ("" = auto-pilot's pool); MapSame: other surface
    bool autoChange = true, visible = true, perspective = true;
    float opacity = 1, brightness = 0, hue = 0, saturation = 1, reactAmt = 0.5f;
    int react = 0;
    float colour[3] = {0.55f, 0.25f, 1.0f};
    // runtime
    int player = -1, token = 0, pendingIdx = -1, curIdx = -1, tries = 0, pw = 256, ph = 256;
    bool pendingCut = false, wantHold = false, noPlayer = false;
    double time = 0;
    unsigned tex = 0;
    RenderTarget hold;  // last frame before a change, faded out over the new preset
    float holdMix = 0;
    std::vector<float> tris;
    bool trisDirty = true;
};
struct MapPlayer {
    PresetPlayer p;
    SDL_Window* w = nullptr;
    SDL_GLContext c = nullptr;
    bool used = false;
};
std::vector<Surface> surfaces;
std::vector<std::unique_ptr<MapPlayer>> mapPlayers;
RenderTarget mapRT;
int mapSel = -1, mapTokens = 0, mapRobin = 0;
bool mapDirty = false, mapIdentify = false, mapDrawing = false;
std::vector<float> mapDraft;
double mapDirtyAt = 0, mapLastTimer = 0;
long long mapLastPhrase = -(1LL << 40);

void markMapDirty() { mapDirty = true, mapDirtyAt = now(); }
std::string mappingPath() { return (fs::u8path(gScenesPath).parent_path() / "mapping.ini").u8string(); }
std::string mapClean(std::string s) {
    for (auto& c : s) if (c == '|' || c == '\n' || c == '\r') c = '/';
    return s;
}

// ---- geometry
std::vector<float> rectPts(float x0, float y0, float x1, float y1) { return {x0, y0, x1, y0, x1, y1, x0, y1}; }
std::vector<float> ellipsePts(float cx, float cy, float rx, float ry, int n = 32) {
    std::vector<float> p;
    for (int i = 0; i < n; i++) {
        double a = i * 6.283185307 / n;
        p.push_back(cx + rx * (float)std::cos(a)), p.push_back(cy + ry * (float)std::sin(a));
    }
    return p;
}
void bboxOf(const std::vector<float>& p, float& x0, float& y0, float& x1, float& y1) {
    x0 = y0 = 1e9f, x1 = y1 = -1e9f;
    for (size_t i = 0; i + 1 < p.size(); i += 2)
        x0 = std::min(x0, p[i]), x1 = std::max(x1, p[i]), y0 = std::min(y0, p[i + 1]), y1 = std::max(y1, p[i + 1]);
    if (x1 < x0) x0 = y0 = 0, x1 = y1 = 1;
}
bool pointInPoly(const std::vector<float>& p, float x, float y) {
    bool in = false;
    int n = (int)p.size() / 2;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float xi = p[i * 2], yi = p[i * 2 + 1], xj = p[j * 2], yj = p[j * 2 + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi + 1e-12f) + xi) in = !in;
    }
    return in;
}
// Ear clipping, so free shapes may be concave (an L, a frame corner, a star). Falls back to a fan.
std::vector<float> triangulate(const std::vector<float>& p) {
    std::vector<float> out;
    int n = (int)p.size() / 2;
    if (n < 3) return out;
    auto X = [&](int i) { return (double)p[i * 2]; };
    auto Y = [&](int i) { return (double)p[i * 2 + 1]; };
    double area = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; area += X(i) * Y(j) - X(j) * Y(i); }
    std::vector<int> v(n);
    for (int i = 0; i < n; i++) v[i] = i;
    if (area < 0) std::reverse(v.begin(), v.end());
    auto cross = [&](int a, int b, int c) { return (X(b) - X(a)) * (Y(c) - Y(a)) - (Y(b) - Y(a)) * (X(c) - X(a)); };
    auto push = [&](int a, int b, int c) { for (int k : {a, b, c}) out.push_back(p[k * 2]), out.push_back(p[k * 2 + 1]); };
    int guard = 0;
    while (v.size() > 3 && guard++ < 4096) {
        bool found = false;
        for (size_t i = 0; i < v.size() && !found; i++) {
            int a = v[(i + v.size() - 1) % v.size()], b = v[i], c = v[(i + 1) % v.size()];
            if (cross(a, b, c) <= 1e-12) continue;  // reflex corner
            bool ear = true;
            for (int q : v)
                if (q != a && q != b && q != c && cross(a, b, q) >= 0 && cross(b, c, q) >= 0 && cross(c, a, q) >= 0) { ear = false; break; }
            if (!ear) continue;
            push(a, b, c);
            v.erase(v.begin() + i);
            found = true;
        }
        if (!found) break;
    }
    for (size_t i = 1; i + 1 < v.size(); i++) push(v[0], v[i], v[i + 1]);
    return out;
}

// ---- players
int acquireMapPlayer() {
    for (size_t i = 0; i < mapPlayers.size(); i++)
        if (!mapPlayers[i]->used) { mapPlayers[i]->used = true; return (int)i; }
    if ((int)mapPlayers.size() >= kMaxMapPlayers) return -1;
    auto mp = std::make_unique<MapPlayer>();
    mp->w = SDL_CreateWindow("", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GL_MakeCurrent(win, ctx);
    mp->c = mp->w ? SDL_GL_CreateContext(mp->w) : nullptr;
    SDL_GL_MakeCurrent(win, ctx);
    if (!mp->c) {
        if (mp->w) SDL_DestroyWindow(mp->w);
        return -1;
    }
    mp->p.setMesh(kQuality[S.quality].meshW, kQuality[S.quality].meshH);
    mp->p.setTexturePaths(texturePaths());
    mp->p.start(mp->w, mp->c);
    mp->used = true;
    mapPlayers.push_back(std::move(mp));
    return (int)mapPlayers.size() - 1;
}
void releaseMapPlayer(Surface& s) {
    if (s.player >= 0 && s.player < (int)mapPlayers.size()) mapPlayers[s.player]->used = false;
    s.player = -1, s.tex = 0, s.curIdx = -1, s.pendingIdx = -1, s.holdMix = 0;
}
void freeSurface(Surface& s) {
    releaseMapPlayer(s);
    if (s.hold.fbo) glDeleteFramebuffers(1, &s.hold.fbo);
    if (s.hold.tex) glDeleteTextures(1, &s.hold.tex);
    s.hold = RenderTarget();
}
void mappingClear() {
    for (auto& s : surfaces) freeSurface(s);
    surfaces.clear();
    mapSel = -1;
}
void mappingPushAudio(const float* s, size_t n) {
    for (auto& mp : mapPlayers)
        if (mp->used) mp->p.pushAudio(s, n);
}
void mappingSetMesh(int w, int h) { for (auto& mp : mapPlayers) mp->p.setMesh(w, h); }
void mappingSetTexturePaths(const std::vector<std::string>& paths) { for (auto& mp : mapPlayers) mp->p.setTexturePaths(paths); }
void mappingShutdown() {
    for (auto& mp : mapPlayers) mp->p.stop();
    for (auto& mp : mapPlayers) {
        if (mp->c) SDL_GL_DeleteContext(mp->c);
        if (mp->w) SDL_DestroyWindow(mp->w);
    }
    mapPlayers.clear();
}

// ---- presets
bool pathInFolder(const std::string& p, const std::string& f) {
    return f.empty() || (p.size() > f.size() && p.compare(0, f.size(), f) == 0 && (p[f.size()] == '\\' || p[f.size()] == '/'));
}
int fitScore(int idx, const std::vector<int>& onScreen, int mood);  // smart picking (05_smart_library)
// A preset for this surface: from its folder (or auto-pilot's pool), not already showing anywhere else,
// and fitting the mood of what is on screen.
int pickForSurface(const Surface& s, int mood = -1) {
    std::vector<int> cand;
    if (!s.folder.empty()) {
        for (int i = 0; i < (int)lib.size(); i++)
            if (!lib[i].broken && pathInFolder(lib[i].path, s.folder)) cand.push_back(i);
    } else {
        AutoPickScope sc;
        for (int i : pickPool())
            if (!lib[i].broken) cand.push_back(i);
    }
    if (cand.empty())
        for (int i = 0; i < (int)lib.size(); i++)
            if (!lib[i].broken) cand.push_back(i);
    if (cand.empty()) return -1;
    std::vector<int> screen = {decks[0].cur, decks[1].cur};
    for (auto& o : surfaces)
        if (&o != &s && o.source == MapOwn) screen.push_back(o.pendingIdx >= 0 ? o.pendingIdx : o.curIdx);
    auto used = [&](int i) { return i == s.curIdx || std::find(screen.begin(), screen.end(), i) != screen.end(); };
    int best = -1, bestScore = -1 << 30;
    for (int k = 0; k < 24; k++) {
        int i = cand[rng() % cand.size()];
        if (used(i) && k < 20) continue;
        int sc = fitScore(i, screen, mood) + (int)(rng() % 12);  // a little chance so it is never the same answer
        if (sc > bestScore) best = i, bestScore = sc;
    }
    return best >= 0 ? best : cand[rng() % cand.size()];
}
void surfaceLoad(Surface& s, int idx, bool cut) {
    if (idx < 0 || idx >= (int)lib.size() || s.source != MapOwn) return;
    if (s.player < 0) s.player = acquireMapPlayer();
    if (s.player < 0) {
        if (!s.noPlayer) toast(true, "Projection map: up to %d surfaces can have their own preset. Use \"Same picture as...\" for the others.", kMaxMapPlayers);
        s.noPlayer = true;
        return;
    }
    s.noPlayer = false;
    s.pendingIdx = idx;
    s.token = ++mapTokens;
    s.pendingCut = cut;
    s.wantHold = !cut && s.tex;
    mapPlayers[s.player]->p.load(lib[idx].path, s.token);
}
// Change a few surfaces at a time so the map keeps evolving without everything jumping at once.
void mappingChangeSome(int mood) {
    std::vector<int> c;
    for (int i = 0; i < (int)surfaces.size(); i++)
        if (surfaces[i].source == MapOwn && surfaces[i].autoChange && surfaces[i].visible) c.push_back(i);
    if (c.empty()) return;
    int n = std::max(1, (int)c.size() / 4);
    for (int k = 0; k < n; k++) {
        Surface& s = surfaces[c[(mapRobin++) % c.size()]];
        surfaceLoad(s, pickForSurface(s, mood), false);
    }
}
void mappingEvent(MusicStructure::Event e) {
    if (!S.autoOn || surfaces.empty()) return;
    bool drop = e == MusicStructure::DropHit;
    if (!drop && e != MusicStructure::SongChange && e != MusicStructure::SongStart && e != MusicStructure::BreakdownStart) return;
    int mood = drop ? 2 : e == MusicStructure::BreakdownStart ? 0 : 1;
    for (auto& s : surfaces)
        if (s.source == MapOwn && s.autoChange && s.visible) surfaceLoad(s, pickForSurface(s, mood), drop);  // drops: everything cuts together
}

// ---- persistence
void mappingSave() {
    mapDirty = false;
    std::ofstream f(fs::u8path(mappingPath()), std::ios::trunc);
    if (!f) return;
    for (auto& s : surfaces) {
        f << "surface=" << mapClean(s.name) << "|" << s.source << "|" << s.preset << "|" << s.folder << "|" << mapClean(s.link) << "|" << s.autoChange << "|"
          << s.visible << "|" << s.perspective << "|" << s.opacity << "|" << s.brightness << "|" << s.hue << "|" << s.saturation << "|" << s.react << "|"
          << s.reactAmt << "|" << s.colour[0] << "|" << s.colour[1] << "|" << s.colour[2] << "|";
        for (size_t i = 0; i < s.pts.size(); i++) f << (i ? "," : "") << s.pts[i];
        f << "\n";
    }
}
void mappingLoad() {
    mappingClear();
    std::ifstream f(fs::u8path(mappingPath()));
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("surface=", 0) != 0) continue;
        auto p = splitBar(line.substr(8));
        if (p.size() < 18) continue;
        Surface s;
        auto F = [&](int i) { return (float)std::atof(p[i].c_str()); };
        s.name = p[0], s.source = std::clamp(std::atoi(p[1].c_str()), 0, MapSourceCount - 1), s.preset = p[2], s.folder = p[3], s.link = p[4];
        s.autoChange = p[5] == "1", s.visible = p[6] == "1", s.perspective = p[7] == "1";
        s.opacity = F(8), s.brightness = F(9), s.hue = F(10), s.saturation = F(11), s.react = std::clamp(std::atoi(p[12].c_str()), 0, 4), s.reactAmt = F(13);
        s.colour[0] = F(14), s.colour[1] = F(15), s.colour[2] = F(16);
        std::stringstream ss(p[17]);
        std::string v;
        while (std::getline(ss, v, ',')) s.pts.push_back((float)std::atof(v.c_str()));
        if (s.pts.size() % 2) s.pts.pop_back();
        if (s.pts.size() >= 6) surfaces.push_back(s);
    }
    mapDirty = false;
}

// ---- rendering
bool mappingNeeded() {
    if (surfaces.empty() || rj.active) return false;
    if (S.showMapping || S.mapPreview) return true;
    for (auto& o : outputs)
        if (o.source == 4) return true;
    return false;
}
unsigned mappingTexture() { return mapRT.tex && !surfaces.empty() ? mapRT.tex : finalRT.tex; }
Surface* surfaceNamed(const std::string& n) {
    for (auto& s : surfaces)
        if (s.name == n) return &s;
    return nullptr;
}

void mappingRender(double t, double dt) {
    int W = std::max(16, renderW), H = std::max(16, renderH);
    mapRT.ensure(W, H);
    double b = beatNow();
    // auto-pilot: a few surfaces change every phrase (or every interval without smart mode)
    if (S.autoOn && structure.section() != MusicStructure::Silence) {
        if (S.smartMix) {
            long long ph = (long long)std::floor(structure.nextBoundary(b, 8));
            if (ph != mapLastPhrase) {
                if (mapLastPhrase > -(1LL << 39) && structure.section() != MusicStructure::Build) mappingChangeSome(structure.section() == MusicStructure::Drop ? 2 : 1);
                mapLastPhrase = ph;
            }
        } else if (t - mapLastTimer > std::max(4, S.interval)) {
            mapLastTimer = t;
            mappingChangeSome(1);
        }
    }
    for (auto& s : surfaces) {
        if (s.trisDirty) s.tris = triangulate(s.pts), s.trisDirty = false;
        if (s.source != MapOwn) {
            if (s.player >= 0) releaseMapPlayer(s);
            continue;
        }
        if (s.player < 0 && !s.noPlayer && !lib.empty()) {
            auto it = byPath.find(s.preset);
            surfaceLoad(s, it != byPath.end() ? it->second : pickForSurface(s), true);
        }
        if (s.player < 0) continue;
        MapPlayer& mp = *mapPlayers[s.player];
        PresetPlayer::LoadResult r;
        while (mp.p.takeResult(r)) {
            if (r.token != s.token || s.pendingIdx < 0) continue;
            int idx = s.pendingIdx;
            s.pendingIdx = -1;
            if (r.ok) {
                s.curIdx = idx;
                if (s.preset != lib[idx].path) s.preset = lib[idx].path, markMapDirty();
                s.holdMix = s.pendingCut || !s.hold.tex ? 0.f : 1.f;
                s.tries = 0;
            } else if (s.tries++ < 3) {
                surfaceLoad(s, pickForSurface(s), true);
            }
        }
        float x0, y0, x1, y1;
        bboxOf(s.pts, x0, y0, x1, y1);
        s.pw = std::clamp((int)((x1 - x0) * W), 64, W) & ~1;
        s.ph = std::clamp((int)((y1 - y0) * H), 64, H) & ~1;
        if (s.wantHold && s.tex) {
            s.hold.ensure(s.pw, s.ph);
            gfx.mix(s.hold, s.tex, s.tex, 0.f, MixCrossfade);
        }
        s.wantHold = false;
        if ((s.visible && !frozen) || mp.p.loading()) {
            if (!frozen) s.time += dt * S.speed;
            mp.p.requestFrame(s.time, s.pw, s.ph);
        }
        s.tex = mp.p.texture();
        s.holdMix = std::max(0.f, s.holdMix - (float)dt / 0.7f);
    }
    mapRT.bind();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    float beatEnv = (float)std::pow(1 - (b - std::floor(b)), 2);
    for (size_t i = 0; i < surfaces.size(); i++) {
        Surface& s = surfaces[i];
        if (!s.visible || s.tris.empty()) continue;
        SurfaceLook l;
        l.opacity = s.opacity, l.hue = s.hue, l.saturation = s.saturation;
        float pulse = s.react == 1 ? bandBass : s.react == 2 ? structure.hat() : s.react == 3 ? structure.snare() : s.react == 4 ? beatEnv : 0.f;
        l.brightness = s.brightness + (s.react ? s.reactAmt * (0.45f * std::min(1.f, pulse) - 0.12f) : 0.f);
        unsigned tex = 0;
        float srcAspect = W / (float)H;
        bool self = false;
        switch (s.source) {
            case MapOwn: tex = s.tex, self = true; break;
            case MapMain: tex = finalRT.tex; break;
            case MapDeckA: tex = decks[0].outTex; break;
            case MapDeckB: tex = decks[1].outTex; break;
            case MapColour: l.useTex = false, memcpy(l.solid, s.colour, sizeof l.solid); break;
            case MapSame:
                if (Surface* o = surfaceNamed(s.link); o && o != &s && o->source == MapOwn) tex = o->tex, srcAspect = o->pw / (float)std::max(1, o->ph);
                break;
        }
        if (s.source != MapColour && !tex) continue;
        float x0, y0, x1, y1;
        bboxOf(s.pts, x0, y0, x1, y1);
        float c[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
        bool quad = s.perspective && s.pts.size() == 8;
        if (quad) memcpy(c, s.pts.data(), sizeof c);
        if (!self && l.useTex) {  // crop the source to the surface's shape instead of squashing it
            float sw = (x1 - x0) * W, sh = (y1 - y0) * H;
            if (quad) {
                auto len = [&](int a, int k) { return std::hypot((c[k * 2] - c[a * 2]) * W, (c[k * 2 + 1] - c[a * 2 + 1]) * H); };
                sw = 0.5f * (len(0, 1) + len(3, 2)), sh = 0.5f * (len(0, 3) + len(1, 2));
            }
            float ta = sw / std::max(1.f, sh);
            if (ta > srcAspect) { float dv = srcAspect / ta; l.uv[1] = (1 - dv) / 2, l.uv[3] = dv; }
            else { float du = ta / srcAspect; l.uv[0] = (1 - du) / 2, l.uv[2] = du; }
        }
        if (mapIdentify && (int)i == mapSel) l.white = 0.45f + 0.45f * (float)std::sin(t * 9);
        gfx.surface(tex, s.tris.data(), (int)s.tris.size() / 2, c, l);
        if (self && s.holdMix > 0.01f && s.hold.tex) {
            SurfaceLook h = l;
            h.opacity = l.opacity * s.holdMix;
            gfx.surface(s.hold.tex, s.tris.data(), (int)s.tris.size() / 2, c, h);
        }
    }
    if (S.mapOutlines)
        for (size_t i = 0; i < surfaces.size(); i++) {
            auto& s = surfaces[i];
            if (s.pts.size() < 6) continue;
            bool sel = (int)i == mapSel;
            float px = std::max(1.5f, H / 360.f) * (sel ? 2.f : 1.f);
            if (sel) gfx.outline(s.pts.data(), (int)s.pts.size() / 2, kAccent.x, kAccent.y, kAccent.z, 1, px, W, H);
            else gfx.outline(s.pts.data(), (int)s.pts.size() / 2, 1, 1, 1, s.visible ? 0.7f : 0.3f, px, W, H);
        }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (mapDirty && now() - mapDirtyAt > 1) mappingSave();
}

// ---- editing
std::string uniqueSurfaceName(const std::string& base) {
    if (!surfaceNamed(base)) return base;
    for (int k = 2;; k++)
        if (!surfaceNamed(base + " " + std::to_string(k))) return base + " " + std::to_string(k);
}
Surface& mapAddSurface(const std::string& name, std::vector<float> pts, bool persp = true, int react = 0) {
    Surface s;
    s.name = uniqueSurfaceName(name);
    s.pts = std::move(pts);
    s.perspective = persp;
    s.react = react;
    surfaces.push_back(s);
    mapSel = (int)surfaces.size() - 1;
    markMapDirty();
    return surfaces.back();
}
float mapAspect() { return renderW > 0 && renderH > 0 ? renderW / (float)renderH : 16.f / 9; }
void mapTemplateDoor() {
    float a = mapAspect();
    mapAddSurface("Wall", rectPts(0.04f, 0.04f, 0.96f, 0.96f), true, 1);
    mapAddSurface("Trim", rectPts(0.335f, 0.12f, 0.665f, 0.96f), true, 3);
    mapAddSurface("Door", rectPts(0.36f, 0.16f, 0.64f, 0.96f), true, 0);
    mapAddSurface("Top panel", rectPts(0.39f, 0.21f, 0.61f, 0.52f), true, 2);
    mapAddSurface("Bottom panel", rectPts(0.39f, 0.58f, 0.61f, 0.91f), true, 2);
    mapAddSurface("Knob", ellipsePts(0.6f, 0.56f, 0.014f, 0.014f * a), false, 4);
}
void mapTemplateGrid() {
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            mapAddSurface("Tile " + std::to_string(y * 3 + x + 1), rectPts(0.05f + x * 0.31f, 0.05f + y * 0.31f, 0.33f + x * 0.31f, 0.33f + y * 0.31f), true,
                          (x + y) % 2 ? 2 : 1);
}
void mapTemplateCube() {
    mapAddSurface("Top face", {0.5f, 0.12f, 0.78f, 0.27f, 0.5f, 0.42f, 0.22f, 0.27f}, true, 2);
    mapAddSurface("Left face", {0.22f, 0.27f, 0.5f, 0.42f, 0.5f, 0.88f, 0.22f, 0.73f}, true, 1);
    mapAddSurface("Right face", {0.5f, 0.42f, 0.78f, 0.27f, 0.78f, 0.73f, 0.5f, 0.88f}, true, 3);
}
void mapDelete(int i) {
    if (i < 0 || i >= (int)surfaces.size()) return;
    freeSurface(surfaces[i]);
    surfaces.erase(surfaces.begin() + i);
    mapSel = std::min(mapSel, (int)surfaces.size() - 1);
    markMapDirty();
}
// snap a point to the nearest corner of another surface (or of this one) within `r` (map units)
void mapSnap(float& x, float& y, int surf, int pt, float rx, float ry) {
    float best = 1;
    for (int i = 0; i < (int)surfaces.size(); i++)
        for (int k = 0; k < (int)surfaces[i].pts.size() / 2; k++) {
            if (i == surf && k == pt) continue;
            float dx = (surfaces[i].pts[k * 2] - x) / rx, dy = (surfaces[i].pts[k * 2 + 1] - y) / ry, d = dx * dx + dy * dy;
            if (d < best) best = d, x = surfaces[i].pts[k * 2], y = surfaces[i].pts[k * 2 + 1];
        }
}

void drawMapCanvas() {
    ImGuiIO& io = ImGui::GetIO();
    float A = mapAspect();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float aw = avail.x, ah = aw / A;
    if (ah > avail.y - 44) ah = std::max(60.f, avail.y - 44), aw = ah * A;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("mapcanvas", ImVec2(aw, ah), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + aw, p0.y + ah), IM_COL32(0, 0, 0, 255));
    if (mapRT.tex) dl->AddImage((ImTextureID)(intptr_t)mapRT.tex, p0, ImVec2(p0.x + aw, p0.y + ah), ImVec2(0, 1), ImVec2(1, 0));
    dl->AddRect(p0, ImVec2(p0.x + aw, p0.y + ah), IM_COL32(255, 255, 255, 40));
    auto scr = [&](float x, float y) { return ImVec2(p0.x + x * aw, p0.y + y * ah); };
    float mx = (io.MousePos.x - p0.x) / aw, my = (io.MousePos.y - p0.y) / ah;
    float rx = 10 / aw, ry = 10 / ah;  // 10 screen pixels in map units
    auto nearPt = [&](float x, float y) { float dx = (x - mx) / rx, dy = (y - my) / ry; return dx * dx + dy * dy < 1; };
    std::vector<ImVec2> tmp;
    for (int i = 0; i < (int)surfaces.size(); i++) {
        auto& s = surfaces[i];
        tmp.clear();
        for (size_t k = 0; k + 1 < s.pts.size(); k += 2) tmp.push_back(scr(s.pts[k], s.pts[k + 1]));
        if (tmp.size() < 2) continue;
        bool sel = i == mapSel;
        dl->AddPolyline(tmp.data(), (int)tmp.size(), sel ? ImGui::GetColorU32(kAccent) : IM_COL32(255, 255, 255, s.visible ? 150 : 60), ImDrawFlags_Closed,
                        sel ? 2.5f : 1.2f);
        float x0, y0, x1, y1;
        bboxOf(s.pts, x0, y0, x1, y1);
        dl->AddText(ImVec2(scr(x0, y0).x + 4, scr(x0, y0).y + 2), sel ? ImGui::GetColorU32(kAccent) : IM_COL32(255, 255, 255, 170), s.name.c_str());
    }
    static int dragPt = -1, nudgePt = -1;
    static bool dragAll = false;
    static float lastX = 0, lastY = 0;
    if (mapDrawing) {
        tmp.clear();
        for (size_t k = 0; k + 1 < mapDraft.size(); k += 2) tmp.push_back(scr(mapDraft[k], mapDraft[k + 1]));
        if (hovered) tmp.push_back(io.MousePos);
        if (tmp.size() >= 2) dl->AddPolyline(tmp.data(), (int)tmp.size(), ImGui::GetColorU32(kAccent2), 0, 2);
        for (size_t k = 0; k + 1 < mapDraft.size(); k += 2)
            dl->AddCircleFilled(scr(mapDraft[k], mapDraft[k + 1]), k == 0 ? 7.f : 4.f, k == 0 ? ImGui::GetColorU32(kAccent2) : IM_COL32(255, 255, 255, 230));
        bool finish = (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)) || (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right));
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (mapDraft.size() >= 6 && nearPt(mapDraft[0], mapDraft[1])) finish = true;
            else {
                float x = mx, y = my;
                if (io.KeyCtrl) mapSnap(x, y, -1, -1, rx, ry);
                mapDraft.push_back(x), mapDraft.push_back(y);
            }
        }
        if (finish && mapDraft.size() >= 6) mapAddSurface("Shape", mapDraft, false), mapDrawing = false, mapDraft.clear();
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) mapDrawing = false, mapDraft.clear();
    } else {
        Surface* s = mapSel >= 0 && mapSel < (int)surfaces.size() ? &surfaces[mapSel] : nullptr;
        int hoverPt = -1;
        if (s)
            for (int k = 0; k < (int)s->pts.size() / 2; k++)
                if (nearPt(s->pts[k * 2], s->pts[k * 2 + 1])) hoverPt = k;
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && s && hoverPt < 0) {  // add a corner on the nearest edge
            int n = (int)s->pts.size() / 2, bestE = -1;
            float bestD = 1;
            for (int k = 0; k < n; k++) {
                float ax = s->pts[k * 2], ay = s->pts[k * 2 + 1], bx = s->pts[(k + 1) % n * 2], by = s->pts[(k + 1) % n * 2 + 1];
                float ex = (bx - ax) / rx, ey = (by - ay) / ry, qx = (mx - ax) / rx, qy = (my - ay) / ry;
                float u = std::clamp((qx * ex + qy * ey) / std::max(1e-6f, ex * ex + ey * ey), 0.f, 1.f);
                float dx = qx - u * ex, dy = qy - u * ey, d = dx * dx + dy * dy;
                if (d < bestD) bestD = d, bestE = k;
            }
            if (bestE >= 0) {
                s->pts.insert(s->pts.begin() + (bestE + 1) * 2, {mx, my});
                s->trisDirty = true;
                markMapDirty();
                dragAll = false;
            }
        } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (hoverPt >= 0) dragPt = nudgePt = hoverPt;
            else {
                int hit = -1;
                for (int i = (int)surfaces.size() - 1; i >= 0 && hit < 0; i--)
                    if (pointInPoly(surfaces[i].pts, mx, my)) hit = i;
                if (hit != mapSel) nudgePt = -1;
                mapSel = hit;
                s = hit >= 0 ? &surfaces[hit] : nullptr;
                dragAll = hit >= 0, lastX = mx, lastY = my;
            }
        }
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && s && hoverPt >= 0 && s->pts.size() > 6) {
            s->pts.erase(s->pts.begin() + hoverPt * 2, s->pts.begin() + hoverPt * 2 + 2);
            s->trisDirty = true, nudgePt = -1;
            markMapDirty();
        }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && s && (dragPt >= 0 || dragAll)) {
            if (dragPt >= 0 && dragPt * 2 + 1 < (int)s->pts.size()) {
                float x = mx, y = my;
                if (io.KeyCtrl) mapSnap(x, y, mapSel, dragPt, rx, ry);
                s->pts[dragPt * 2] = std::clamp(x, -0.25f, 1.25f), s->pts[dragPt * 2 + 1] = std::clamp(y, -0.25f, 1.25f);
            } else if (dragAll) {
                for (size_t k = 0; k + 1 < s->pts.size(); k += 2) s->pts[k] += mx - lastX, s->pts[k + 1] += my - lastY;
                lastX = mx, lastY = my;
            }
            s->trisDirty = true;
        } else {
            if (dragPt >= 0 || dragAll) markMapDirty();
            dragPt = -1, dragAll = false;
        }
        // arrow keys nudge the picked corner (or the whole surface) by one output pixel; Shift = 10
        if (s && ImGui::IsWindowFocused() && !io.WantTextInput) {
            float step = (io.KeyShift ? 10.f : 1.f), dx = 0, dy = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) dx = -step;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) dx = step;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) dy = -step;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) dy = step;
            if (dx != 0 || dy != 0) {
                dx /= std::max(16, renderW), dy /= std::max(16, renderH);
                if (nudgePt >= 0 && nudgePt * 2 + 1 < (int)s->pts.size()) s->pts[nudgePt * 2] += dx, s->pts[nudgePt * 2 + 1] += dy;
                else for (size_t k = 0; k + 1 < s->pts.size(); k += 2) s->pts[k] += dx, s->pts[k + 1] += dy;
                s->trisDirty = true;
                markMapDirty();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && nudgePt < 0) { mapDelete(mapSel); s = nullptr; }
        }
        if (s)
            for (int k = 0; k < (int)s->pts.size() / 2; k++) {
                bool hot = k == hoverPt || k == dragPt, picked = k == nudgePt;
                ImVec2 c = scr(s->pts[k * 2], s->pts[k * 2 + 1]);
                dl->AddCircleFilled(c, hot ? 8.f : 6.f, hot || picked ? ImGui::GetColorU32(kAccent2) : IM_COL32(255, 255, 255, 230));
                dl->AddCircle(c, hot ? 8.f : 6.f, IM_COL32(0, 0, 0, 200));
            }
    }
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + ah + 4));
    if (mapDrawing)
        ImGui::TextColored(kAccent2, "Click to add corners. Click the first corner, press Enter or right-click to finish. Esc cancels. Ctrl snaps.");
    else
        ImGui::TextDisabled("Click a surface to pick it, drag it to move. Drag corners (Ctrl snaps to other corners), double-click an edge to add a corner,\n"
                            "right-click a corner to remove it. Arrow keys nudge by one pixel (Shift: 10). Lower in the list = drawn on top.");
}

void drawSurfaceProps(Surface& s) {
    char nb[96];
    snprintf(nb, sizeof nb, "%s", s.name.c_str());
    ImGui::SetNextItemWidth(-60);
    if (ImGui::InputText("Name", nb, sizeof nb)) {
        std::string old = s.name;
        s.name = mapClean(nb);
        for (auto& o : surfaces)
            if (o.link == old) o.link = s.name;
        markMapDirty();
    }
    ImGui::SetNextItemWidth(-60);
    if (ImGui::Combo("Shows", &s.source, kMapSourceNames, MapSourceCount)) s.noPlayer = false, markMapDirty();
    if (s.source == MapOwn) {
        auto it = byPath.find(s.preset);
        ImGui::TextWrapped("Preset: %s", s.pendingIdx >= 0 ? "loading..." : it != byPath.end() ? lib[it->second].name.c_str() : "(choosing...)");
        if (ImGui::Button("Random")) surfaceLoad(s, pickForSurface(s), false);
        ImGui::SameLine();
        if (ImGui::Button("Next")) {
            const auto& pool = view;
            if (!pool.empty()) {
                auto f = std::find(pool.begin(), pool.end(), s.curIdx);
                surfaceLoad(s, pool[f == pool.end() ? 0 : (f - pool.begin() + 1) % pool.size()], false);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Deck A's") && decks[0].cur >= 0) surfaceLoad(s, decks[0].cur, false);
        ImGui::SameLine();
        if (ImGui::Button("Deck B's") && decks[1].cur >= 0) surfaceLoad(s, decks[1].cur, false);
        if (ImGui::Checkbox("Auto-pilot changes it", &s.autoChange)) markMapDirty();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("While auto-pilot runs: every surface cuts to a new preset on drops,\nblends on breakdowns and new songs, and a few change each phrase.");
        if (autoFoldersFor != lib.size()) buildAutoFolders();
        std::string label = "Auto-pilot's pool";
        for (auto& [l, p] : autoFolders)
            if (p == s.folder) label = l;
        ImGui::SetNextItemWidth(-60);
        if (ImGui::BeginCombo("Pick from", label.c_str())) {
            if (ImGui::Selectable("Auto-pilot's pool", s.folder.empty())) s.folder.clear(), markMapDirty();
            for (auto& [l, p] : autoFolders)
                if (ImGui::Selectable(l.c_str(), p == s.folder)) s.folder = p, markMapDirty();
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Or right-click a preset in the browser > Send to surface.");
    } else if (s.source == MapSame) {
        ImGui::SetNextItemWidth(-60);
        if (ImGui::BeginCombo("Surface", s.link.empty() ? "(pick one)" : s.link.c_str())) {
            for (auto& o : surfaces)
                if (&o != &s && o.source == MapOwn && ImGui::Selectable(o.name.c_str(), o.name == s.link)) s.link = o.name, markMapDirty();
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Shares that surface's preset (costs nothing extra).");
    } else if (s.source == MapColour) {
        if (ImGui::ColorEdit3("Colour", s.colour)) markMapDirty();
    }
    ImGui::SeparatorText("Look");
    bool ch = false;
    ImGui::SetNextItemWidth(-90);
    ch |= ImGui::SliderFloat("Opacity", &s.opacity, 0.f, 1.f, "%.2f");
    ImGui::SetNextItemWidth(-90);
    ch |= ImGui::SliderFloat("Brightness", &s.brightness, -0.5f, 0.5f, "%.2f");
    ImGui::SetNextItemWidth(-90);
    ch |= ImGui::SliderFloat("Hue shift", &s.hue, 0.f, 1.f, "%.2f");
    ImGui::SetNextItemWidth(-90);
    ch |= ImGui::SliderFloat("Saturation", &s.saturation, 0.f, 2.f, "%.2f");
    ImGui::SetNextItemWidth(-90);
    ch |= ImGui::Combo("Pulses with", &s.react, kMapReactNames, 5);
    if (s.react) {
        ImGui::SetNextItemWidth(-90);
        ch |= ImGui::SliderFloat("Pulse", &s.reactAmt, 0.f, 1.f, "%.2f");
    }
    ImGui::SeparatorText("Shape");
    if (s.pts.size() == 8) {
        ch |= ImGui::Checkbox("Perspective (picture follows the 4 corners)", &s.perspective);
    } else {
        ImGui::TextDisabled("%d corners", (int)s.pts.size() / 2);
    }
    if (ImGui::Button("Make rectangle")) {
        float x0, y0, x1, y1;
        bboxOf(s.pts, x0, y0, x1, y1);
        s.pts = rectPts(x0, y0, x1, y1), s.trisDirty = true, ch = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Make circle")) {
        float x0, y0, x1, y1;
        bboxOf(s.pts, x0, y0, x1, y1);
        s.pts = ellipsePts((x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2), s.trisDirty = true, ch = true;
    }
    if (ch) markMapDirty();
}

void drawMapping() {
    ImGui::SetNextWindowSize(ImVec2(1120, 740), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Projection mapping", &S.showMapping, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    float A = mapAspect();
    if (ImGui::Button("+ Rectangle")) mapAddSurface("Rectangle", rectPts(0.35f, 0.3f, 0.65f, 0.7f));
    ImGui::SameLine();
    if (ImGui::Button("+ Free shape")) mapDrawing = true, mapDraft.clear();
    ImGui::SameLine();
    if (ImGui::Button("+ Circle")) mapAddSurface("Circle", ellipsePts(0.5f, 0.5f, 0.1f, 0.1f * A), false);
    ImGui::SameLine();
    if (ImGui::Button("Templates")) ImGui::OpenPopup("maptpl");
    if (ImGui::BeginPopup("maptpl")) {
        if (ImGui::MenuItem("Door (wall, trim, door, panels, knob)")) mapTemplateDoor();
        if (ImGui::MenuItem("3 x 3 grid")) mapTemplateGrid();
        if (ImGui::MenuItem("Cube corner (3 faces)")) mapTemplateCube();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) ImGui::OpenPopup("mapclear");
    if (ImGui::BeginPopup("mapclear")) {
        ImGui::Text("Remove all %d surfaces?", (int)surfaces.size());
        if (ImGui::Button("Remove all")) { mappingClear(); markMapDirty(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SameLine(0, 24);
    ImGui::Checkbox("Outlines", &S.mapOutlines);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Draw every surface's edge on the outputs too, to line them up with the real object.");
    ImGui::SameLine();
    ImGui::Checkbox("Flash selected", &mapIdentify);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make the selected surface blink white on the projector so you can find it.");
    ImGui::SameLine();
    ImGui::Checkbox("Map in main window", &S.mapPreview);
    int onOut = 0;
    for (auto& o : outputs) onOut += o.source == 4;
    ImGui::SameLine();
    if (onOut) ImGui::TextColored(kAccent2, "On %d output%s", onOut, onOut > 1 ? "s" : "");
    else if (ImGui::Button("Send map to an output")) {
        if (outputs.empty()) openOutput(SDL_GetNumVideoDisplays() > 1 ? -2 : -1, 4);
        else outputs[0].source = 4;
    }
    int own = 0;
    for (auto& s : surfaces) own += s.source == MapOwn;
    ImGui::SameLine();
    ImGui::TextDisabled("%d surfaces, %d/%d with their own preset", (int)surfaces.size(), std::min(own, kMaxMapPlayers), kMaxMapPlayers);

    ImGui::BeginChild("mapleft", ImVec2(340, 0), ImGuiChildFlags_Borders);
    ImGui::BeginChild("maplist", ImVec2(0, std::min(220.f, 26.f * surfaces.size() + 12)), ImGuiChildFlags_None);
    for (int i = 0; i < (int)surfaces.size(); i++) {
        auto& s = surfaces[i];
        ImGui::PushID(i);
        if (ImGui::Checkbox("##vis", &s.visible)) markMapDirty();
        ImGui::SameLine();
        std::string what = s.source == MapOwn ? (s.curIdx >= 0 ? lib[s.curIdx].name : std::string("...")) : s.source == MapSame ? "= " + s.link : kMapSourceNames[s.source];
        if (ImGui::Selectable((s.name + "##s").c_str(), i == mapSel, 0, ImVec2(120, 0))) mapSel = i, mapDrawing = false;
        ImGui::SameLine();
        ImGui::TextDisabled("%s", what.c_str());
        ImGui::PopID();
    }
    if (surfaces.empty()) ImGui::TextWrapped("No surfaces yet. Add a rectangle, draw a free shape, or start from a template (try Door).");
    ImGui::EndChild();
    bool has = mapSel >= 0 && mapSel < (int)surfaces.size();
    ImGui::BeginDisabled(!has || mapSel == 0);
    if (ImGui::ArrowButton("up", ImGuiDir_Up)) std::swap(surfaces[mapSel], surfaces[mapSel - 1]), mapSel--, markMapDirty();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!has || mapSel + 1 >= (int)surfaces.size());
    if (ImGui::ArrowButton("down", ImGuiDir_Down)) std::swap(surfaces[mapSel], surfaces[mapSel + 1]), mapSel++, markMapDirty();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!has);
    if (ImGui::Button("Duplicate")) {
        Surface c = surfaces[mapSel];
        c.player = -1, c.tex = 0, c.curIdx = -1, c.pendingIdx = -1, c.hold = RenderTarget(), c.holdMix = 0, c.trisDirty = true;
        c.name = uniqueSurfaceName(c.name);
        for (size_t k = 0; k + 1 < c.pts.size(); k += 2) c.pts[k] += 0.03f, c.pts[k + 1] += 0.03f;
        surfaces.push_back(c);
        mapSel = (int)surfaces.size() - 1;
        markMapDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) mapDelete(mapSel);
    ImGui::EndDisabled();
    ImGui::Separator();
    if (mapSel >= 0 && mapSel < (int)surfaces.size()) drawSurfaceProps(surfaces[mapSel]);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("mapcanvaswrap", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    drawMapCanvas();
    ImGui::EndChild();
    ImGui::End();
}

// browser right-click menu: put a preset on a surface
void mappingContextMenu(int idx) {
    if (surfaces.empty()) return;
    if (ImGui::BeginMenu("Send to surface")) {
        for (auto& s : surfaces)
            if (ImGui::MenuItem(s.name.c_str())) {
                if (s.source != MapOwn) s.source = MapOwn, s.noPlayer = false;
                s.autoChange = false;  // the user chose it: auto-pilot leaves it alone
                surfaceLoad(s, idx, false);
                markMapDirty();
                toast(false, "%s on %s (auto changes off for this surface)", lib[idx].name.c_str(), s.name.c_str());
            }
        ImGui::EndMenu();
    }
}

int mapOutIdx = -1;
std::vector<int> mapBefore;
void addMappingTests(const std::function<void(double, std::function<void()>)>& at) {
    at(0.5, [] {
        mappingClear();
        mapTemplateDoor();
        surfaces[1].source = MapSame, surfaces[1].link = "Bottom panel";  // trim shares a picture
        S.showMapping = true, S.mapOutlines = true;
        mapSel = 3;
        Output* o = openOutput(-1, 4);
        mapOutIdx = o ? (int)outputs.size() - 1 : -1;
    });
    at(5.0, [] {
        int own = 0, loaded = 0;
        std::set<int> distinct;
        for (auto& s : surfaces)
            if (s.source == MapOwn) own++, loaded += s.curIdx >= 0 && s.tex != 0, distinct.insert(s.curIdx);
        int players = 0;
        for (auto& mp : mapPlayers) players += mp->used;
        testNote("mapping: surfaces=" + std::to_string(surfaces.size()) + " own=" + std::to_string(own) + " loaded=" + std::to_string(loaded) +
                 " distinct=" + std::to_string(distinct.size()) + " players=" + std::to_string(players) + " tris(knob)=" + std::to_string(surfaces[5].tris.size() / 6));
        pendingShot = "28-mapping-window";
        if (mapOutIdx == 0) pendingOutShot = "29-mapping-output";
    });
    at(0.5, [] {
        S.autoOn = true;
        mapBefore.clear();
        for (auto& s : surfaces) mapBefore.push_back(s.curIdx);
        mappingEvent(MusicStructure::DropHit);
    });
    at(3.0, [] {
        int changed = 0, own = 0;
        for (size_t i = 0; i < surfaces.size() && i < mapBefore.size(); i++)
            if (surfaces[i].source == MapOwn) own++, changed += surfaces[i].curIdx >= 0 && surfaces[i].curIdx != mapBefore[i];
        testNote("mapping drop: changed=" + std::to_string(changed) + "/" + std::to_string(own));
        S.autoOn = false;
        pendingShot = "30-mapping-after-drop";
    });
    at(0.5, [] {
        mappingSave();
        size_t n = surfaces.size();
        std::string knob = surfaces.back().name;
        mappingLoad();
        testNote("mapping reload: " + std::to_string(n) + " -> " + std::to_string(surfaces.size()) + " last=" + (surfaces.empty() ? "?" : surfaces.back().name) +
                 " (was " + knob + ")");
        if (mapOutIdx >= 0 && mapOutIdx < (int)outputs.size()) closeOutput(mapOutIdx);
        S.showMapping = false;
    });
    at(0.5, [] { mappingClear(); mappingSave(); });
}
