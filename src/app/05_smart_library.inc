// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ---- smart library: analysis results, colour/energy pools, similarity
struct Look { float hue = 0, sat = 0, bright = 0, motion = 0, punch = -1; };  // punch -1 = not measured yet
void updateEnergyCuts();
std::unordered_map<std::string, Look> looks;
bool looksDirty = false, energyDirty = true;
double lastLookSave = 0;
const char* kColorNames[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink", "Mono"};
const char* colorOf(const Look& l) {
    if (l.sat < 0.18f) return "Mono";
    float h = l.hue;
    if (h < 0.04f || h >= 0.94f) return "Red";
    if (h < 0.11f) return "Orange";
    if (h < 0.19f) return "Yellow";
    if (h < 0.45f) return "Green";
    if (h < 0.55f) return "Cyan";
    if (h < 0.72f) return "Blue";
    if (h < 0.83f) return "Purple";
    return "Pink";
}
float lookDistance(const Look& a, const Look& b) {
    float dh = std::fabs(a.hue - b.hue);
    dh = std::min(dh, 1 - dh) * 2 * std::min(a.sat, b.sat) * 2;  // hue matters only for vivid presets
    float ds = a.sat - b.sat, db = a.bright - b.bright, dm = a.motion - b.motion;
    return dh * dh + ds * ds + db * db * 1.5f + dm * dm * 2;
}
void saveLooks() {
    std::ofstream f(fs::u8path(gAnalysisPath), std::ios::trunc);
    for (auto& [p, l] : looks) f << p << "|" << l.hue << "|" << l.sat << "|" << l.bright << "|" << l.motion << "|" << l.punch << "\n";
    looksDirty = false;
}
void loadLooks() {
    std::ifstream f(fs::u8path(gAnalysisPath));
    std::string line;
    while (std::getline(f, line)) {
        auto p = splitBar(line);
        if (p.size() != 5 && p.size() != 6) continue;
        looks[p[0]] = {(float)std::atof(p[1].c_str()), (float)std::atof(p[2].c_str()), (float)std::atof(p[3].c_str()), (float)std::atof(p[4].c_str()),
                       p.size() == 6 ? (float)std::atof(p[5].c_str()) : -1.f};
    }
}
int analysisQueued = 0;
bool analyseNow = false;  // run the analysis even while performing (button in the browser)
// Presets auto-pilot can pick right now (its folder, then the browser's pool) are analysed first.
void queueAnalysis() {
    std::vector<std::string> todo;
    std::vector<char> first(lib.size(), 0);
    for (int i : autoView) first[i] = 2;
    for (int i : view) first[i] = std::max<char>(first[i], 1);
    for (int pass = 2; pass >= 0; pass--)
        for (size_t i = 0; i < lib.size(); i++)
            if (first[i] == pass && !lib[i].broken && !looks.count(lib[i].path)) todo.push_back(lib[i].path);
    for (auto& p : lib) {  // then measure bass punch for presets analysed by older versions
        auto it = looks.find(p.path);
        if (!p.broken && it != looks.end() && it->second.punch < 0) todo.push_back(p.path);
    }
    thumbs.analysisEnabled = S.analysisOn;
    analysisQueued = (int)todo.size();
    thumbs.queueAnalysis(std::move(todo));
}
void intakeAnalysis(double t) {
    for (auto& a : thumbs.takeAnalysis()) {
        if (a.failed) {
            auto it = byPath.find(a.path);
            if (it != byPath.end()) markBroken(it->second, "failed during analysis");
            continue;
        }
        looks[a.path] = {a.hue, a.sat, a.bright, a.motion, a.punch};
        looksDirty = true;
        energyDirty = true;
    }
    if (looksDirty && t - lastLookSave > 20) { lastLookSave = t; saveLooks(); }
    static double lastCuts = -100;
    if (energyDirty && t - lastCuts > 10) { lastCuts = t; updateEnergyCuts(); }
}
// Energy classes, relative to this library: intense = hits hard on kicks (top ~30% by punch, not static),
// calm = little motion and little punch. Presets without a punch score fall back to motion only.
struct EnergyCuts { float punchHi = 2, punchMid = 2, motionLo = 0.12f; bool punchKnown = false; } ecuts;
void updateEnergyCuts() {
    std::vector<float> p, m;
    for (auto& [k, l] : looks) {
        m.push_back(l.motion);
        if (l.punch >= 0) p.push_back(l.punch);
    }
    auto pct = [](std::vector<float>& v, float q) { size_t i = std::min(v.size() - 1, (size_t)(q * v.size())); std::nth_element(v.begin(), v.begin() + i, v.end()); return v[i]; };
    if (m.size() >= 20) ecuts.motionLo = pct(m, 0.3f);
    ecuts.punchKnown = p.size() >= 20;
    if (ecuts.punchKnown) ecuts.punchHi = pct(p, 0.7f), ecuts.punchMid = pct(p, 0.5f);
    energyDirty = false;
}
int energyClass(const Look& l) {
    if (l.punch >= 0 && ecuts.punchKnown) {
        if (l.punch >= ecuts.punchHi && l.motion > ecuts.motionLo) return 1;
        if (l.motion <= ecuts.motionLo && l.punch < ecuts.punchMid) return -1;
        return 0;
    }
    return l.motion > 0.35f ? 1 : l.motion < 0.12f ? -1 : 0;
}
// pools understood by inPool(): "color:<Name>", "energy:calm|intense", "light:dark|bright", "similar:<path>"
bool smartPoolMatch(int i) {
    auto it = looks.find(lib[i].path);
    if (it == looks.end()) return false;
    const Look& l = it->second;
    if (S.pool.rfind("color:", 0) == 0) return S.pool.substr(6) == colorOf(l);
    if (S.pool == "energy:calm") return energyClass(l) < 0;
    if (S.pool == "energy:intense") return energyClass(l) > 0;
    if (S.pool == "light:dark") return l.bright < 0.18f;
    if (S.pool == "light:bright") return l.bright > 0.42f;
    return true;
}


