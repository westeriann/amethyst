// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).
// ---- smart auto-mix: follows the song structure, prepares the next preset ahead of time and
// switches exactly on the bar / drop.
struct Smart {
    bool prep = false;
    int deck = -1, idx = -1;
    int fireOn = 0;  // 0 = at fireBeat, 1 = on the next drop, 2 = when the next song starts
    double fireBeat = 0, blend = 0;
    bool hard = false;
    double lastSwitch = 0;
    bool afterFake = false;  // between a fakeout and the real drop
    bool peeking = false;    // the drop preset is already faded in a little (build-up tease)
    int energy = 0;          // energy class of the prepared preset
    std::string lastEvent;
    double lastEventAt = -100;
} smart;

int onAirDeck() { return S.xfade < 0.5f ? 0 : 1; }

// ---- how well a preset fits right now. Higher = better. Weighs, in order of how often they go wrong:
// too bright for the moment, the wrong energy for the section, clashing with what is on screen, leaving
// the song's colour palette, motion that ignores the tempo, and what you think of it (favourites, ratings,
// presets you skip straight away, presets that just played).
struct PickStat { int skips = 0, likes = 0; };
std::unordered_map<std::string, PickStat> pickStats;
std::vector<int> recentPicks;  // last auto picks, oldest first
struct Vibe { float hue = 0, sat = 0, bright = 0.3f; bool set = false; } vibe;  // this song's palette so far
int lastAutoPick = -1;
std::string pickStatsPath() { return (fs::u8path(gAnalysisPath).parent_path() / "pickstats.ini").u8string(); }
void savePickStats() {
    std::ofstream f(fs::u8path(pickStatsPath()), std::ios::trunc);
    for (auto& [p, s] : pickStats) f << p << "|" << s.skips << "|" << s.likes << "\n";
}
void loadPickStats() {
    std::ifstream f(fs::u8path(pickStatsPath()));
    std::string line;
    while (std::getline(f, line)) {
        auto p = splitBar(line);
        if (p.size() == 3) pickStats[p[0]] = {std::atoi(p[1].c_str()), std::atoi(p[2].c_str())};
    }
}
// called by loadPreset for manual loads while auto-pilot runs
void notePickManual(int replaced, double shownFor, int chosen) {
    if (replaced >= 0 && replaced == lastAutoPick && shownFor < 15) pickStats[lib[replaced].path].skips++;  // you skipped its pick
    if (chosen >= 0) { auto& s = pickStats[lib[chosen].path]; s.likes = std::min(s.likes + 1, 20); }
    savePickStats();
}
int moodNow() {
    switch (structure.section()) {
        case MusicStructure::Drop: return 2;
        case MusicStructure::Breakdown: case MusicStructure::Intro: return 0;
        case MusicStructure::Groove: return structure.energy() > 0.9f ? 2 : 1;
        default: return 1;
    }
}
// mood: 0 calm (breakdown), 1 groove, 2 drop; -1 = from the song section now
int fitScore(int idx, const std::vector<int>& screen, int mood) {
    if (idx < 0 || idx >= (int)lib.size()) return -(1 << 20);
    if (mood < 0) mood = moodNow();
    const std::string& path = lib[idx].path;
    int sc = 0;
    for (size_t k = 0; k < recentPicks.size(); k++)
        if (recentPicks[k] == idx) sc -= 150 + 10 * (int)k;  // just played: the more recent, the worse
    if (auto ps = pickStats.find(path); ps != pickStats.end()) sc += -110 * std::min(ps->second.skips, 4) + 25 * std::min(ps->second.likes, 4);
    if (auto m = meta.find(path); m != meta.end()) {
        if (m->second.fav) sc += 70;
        if (m->second.rating > 0) sc += 35 * (m->second.rating - 3);
    }
    auto it = looks.find(path);
    if (it == looks.end()) return sc - (S.journey ? 260 : 140);  // not analysed yet: a risk (bigger when a mood is set)
    const Look& l = it->second;
    int want = mood == 2 ? 1 : mood == 0 ? -1 : 0, ec = energyClass(l);
    sc += ec == want ? 110 : std::abs(ec - want) == 1 ? 0 : -170;
    // bright is fine; a white-out where you can't make anything out is not. Steep penalty only near white,
    // plus a gentle one for very bright presets in breakdowns.
    float whiteOut = mood == 2 ? 0.8f : 0.76f;
    if (l.bright > whiteOut) sc -= (int)((l.bright - whiteOut) * 3500);
    if (mood == 0 && l.bright > 0.65f) sc -= (int)((l.bright - 0.65f) * 500);
    if (l.bright < 0.05f) sc -= 120;  // nearly black looks like a fault on a projector
    float dSum = 0;
    int dN = 0;
    for (int o : screen) {
        if (o < 0 || o == idx) continue;
        auto oi = looks.find(lib[o].path);
        if (oi == looks.end()) continue;
        float d = lookDistance(l, oi->second);
        dSum += std::min(d, 1.f), dN++;
        if (d < 0.003f) sc -= 60;  // a near-copy of what is already showing
    }
    if (dN) sc -= (int)(dSum / dN * (mood == 2 ? 120 : 260));  // drops may jump further
    if (vibe.set) {
        Look v{vibe.hue, vibe.sat, vibe.bright, l.motion, -1};
        sc -= (int)(std::min(lookDistance(l, v), 1.f) * 170);
    }
    float bpm = (float)tempo.bpm();
    if (bpm > 0) {
        float target = (0.12f + 0.3f * std::clamp((bpm - 80) / 100, 0.f, 1.f)) * (mood == 0 ? 0.6f : mood == 2 ? 1.3f : 1.f);
        sc -= (int)(std::fabs(l.motion - target) * 160);
    }
    if (const Journey* j = journeyNow()) {  // the set's mood outweighs most other wishes
        if (j->nh > 0) {
            float d = 1;
            for (int k = 0; k < j->nh; k++) {
                float dh = std::fabs(l.hue - j->hues[k]);
                d = std::min(d, std::min(dh, 1 - dh));
            }
            float vivid = std::clamp(l.sat * 2.5f, 0.f, 1.f);  // grey presets have no real colour to judge
            sc -= (int)((std::max(0.f, d - 0.06f) * 4 * vivid + (1 - vivid) * 0.35f) * 380);
        }
        float bOff = l.bright - j->bright + (mood == 2 ? -0.08f : 0.f);  // drops may be a bit brighter
        sc -= (int)(std::fabs(bOff) * (bOff > 0 ? 420 : 300));
        sc -= (int)(std::fabs(l.sat - j->sat) * 220);
        sc -= (int)(std::fabs(l.motion - j->motion) * 200);
        if (j->energyBias) sc += ec == j->energyBias ? 70 : ec == -j->energyBias ? -90 : 0;
    }
    return sc;
}
void notePick(int idx) {
    lastAutoPick = idx;
    recentPicks.push_back(idx);
    if (recentPicks.size() > 40) recentPicks.erase(recentPicks.begin());
    auto it = looks.find(lib[idx].path);
    if (it == looks.end()) return;
    const Look& l = it->second;
    if (!vibe.set) { vibe = {l.hue, l.sat, l.bright, true}; return; }
    float dh = l.hue - vibe.hue;
    dh -= std::round(dh);  // shortest way round the colour wheel
    vibe.hue = vibe.hue + 0.25f * dh, vibe.hue -= std::floor(vibe.hue);
    vibe.sat += 0.25f * (l.sat - vibe.sat), vibe.bright += 0.25f * (l.bright - vibe.bright);
}
// best of up to 80 candidates from the pool, with a little chance mixed in so it never gets predictable
int smartPick(int energy) {
    AutoPickScope scope;
    int on = onAirDeck();
    if (energy == 0 && !S.shuffle) return stepFrom(decks[on].cur, 1);
    const auto& pool = pickPool();
    if (pool.empty()) return -1;
    int mood = energy > 0 ? 2 : energy < 0 ? 0 : -1;
    std::vector<int> screen = {decks[0].cur, decks[1].cur};
    int best = -1, bestSc = -(1 << 30);
    bool all = pool.size() <= 80;
    for (size_t k = 0; k < std::min<size_t>(pool.size(), 80); k++) {
        int i = all ? pool[k] : pool[rng() % pool.size()];
        if (lib[i].broken || i == decks[0].cur || i == decks[1].cur) continue;
        int sc = fitScore(i, screen, mood) + (int)(rng() % 50);
        if (sc > bestSc) bestSc = sc, best = i;
    }
    if (best < 0) best = pickRandom(decks[on].cur);
    if (best >= 0) notePick(best);
    return best;
}

bool releaseHeld(int d, double blendSecs) {
    Deck& dk = decks[d];
    if (dk.heldIdx < 0) return false;
    int idx = dk.heldIdx;
    dk.heldIdx = -1;
    if (dk.cur >= 0 && dk.cur != idx) {
        dk.hist.push_back(lib[dk.cur].path);
        if (dk.hist.size() > 300) dk.hist.erase(dk.hist.begin());
    }
    dk.cur = idx;
    dk.lastSwitch = now();
    (d ? S.lastB : S.lastA) = lib[idx].path;
    dk.active = 1 - dk.active;
    dk.blending = blendSecs > 0.01;
    dk.blendStart = now();
    dk.blendDur = blendSecs;
    dk.blendMode = MixCrossfade;
    return true;
}

void smartCancel() {
    if (smart.prep && smart.deck >= 0) {
        Deck& dk = decks[smart.deck];
        if (dk.heldIdx == smart.idx) dk.heldIdx = -1;
        if (dk.pendingIdx == smart.idx && dk.pendingHold) { dk.pendingIdx = -1; dk.pendingToken = 0; dk.pendingHold = false; }
    }
    if (smart.prep && smart.peeking) {  // a tease is showing: finish it gently instead of snapping back
        if (S.autoDecks) startXfade(smart.deck ? 1.f : 0.f, 2.0);
        else {
            Deck& dk = decks[smart.deck];
            dk.blendStart = now() - std::max(0.f, dk.blendManual) * 2.0, dk.blendDur = 2.0, dk.blendManual = -1, dk.blendMode = MixCrossfade;
        }
    }
    smart.peeking = false;
    smart.prep = false;
}
bool smartValid() {
    Deck& dk = decks[smart.deck];
    if (smart.peeking) return dk.cur == smart.idx;
    if (S.autoDecks) return dk.pendingIdx == smart.idx || dk.cur == smart.idx;
    return (dk.pendingIdx == smart.idx && dk.pendingHold) || dk.heldIdx == smart.idx;
}
bool smartReady() {
    Deck& dk = decks[smart.deck];
    if (smart.peeking) return true;
    return S.autoDecks ? dk.pendingIdx < 0 && dk.cur == smart.idx : dk.heldIdx == smart.idx;
}
bool smartPrepare(int energy, int fireOn, double fireBeat, double blend, bool hard) {
    AutoPickScope autoScope;  // this load is auto-pilot's, not a manual skip
    smartCancel();
    int on = onAirDeck(), d = S.autoDecks ? 1 - on : on;
    if (S.autoDecks ? xanim.active : decks[d].blending) return false;  // the spare is still in use
    int idx = smartPick(energy);
    if (idx < 0 || !loadPreset(d, idx, true)) return false;
    if (!S.autoDecks) decks[d].pendingHold = true;
    smart.prep = true;
    smart.deck = d, smart.idx = idx, smart.fireOn = fireOn, smart.fireBeat = fireBeat, smart.blend = blend, smart.hard = hard;
    smart.energy = energy;
    return true;
}
const int smashModes[] = {MixAdd, MixScreen, MixDifference, MixLighten};
int smashMode = -1;
double smashUntil = 0;
void smartPeek(float k) {  // glow the waiting drop preset in a little
    if (!smart.prep || !smartReady()) return;
    if (S.autoDecks) {
        S.xfade = smart.deck ? k : 1 - k;
    } else {
        Deck& dk = decks[smart.deck];
        if (!smart.peeking) {
            releaseHeld(smart.deck, 0);
            dk.blending = true;
            dk.blendMode = S.smartBlends ? MixScreen : MixCrossfade;
        }
        dk.blendManual = k;
    }
    smart.peeking = true;
}
void smartFire(double t) {
    // drops: smash the two presets together with a layer blend for a bar instead of a plain cut
    bool smash = smart.hard && autoFxOn() && S.smartBlends && smart.energy > 0;
    int mode = smashModes[rng() % 4];
    {
        auto lb = [](int i) { auto it = i >= 0 ? looks.find(lib[i].path) : looks.end(); return it == looks.end() ? 0.3f : it->second.bright; };
        int a = decks[onAirDeck()].cur, b = smart.idx;
        if (lb(a) + lb(b) > 0.75f && (mode == MixAdd || mode == MixScreen)) mode = rng() % 2 ? MixLighten : MixDifference;
    }
    if (S.autoDecks) {
        if (smash) {
            S.xfade = 0.5f;  // both decks full
            smashMode = mode, smashUntil = t + beatsToSec(4);
            startXfade(smart.deck ? 1.f : 0.f, beatsToSec(4));
        } else {
            startXfade(smart.deck ? 1.f : 0.f, smart.hard ? 0.04 : smart.blend);
        }
    } else {
        Deck& dk = decks[smart.deck];
        if (smart.peeking) {
            float k = std::max(0.f, dk.blendManual);
            dk.blendManual = -1;
            if (smash) dk.blending = true, dk.blendStart = now(), dk.blendDur = beatsToSec(4), dk.blendMode = mode;
            else if (smart.hard) dk.blending = false, dk.blendMode = MixCrossfade;
            else dk.blendStart = now() - k * smart.blend, dk.blendDur = smart.blend, dk.blendMode = MixCrossfade;
        } else {
            releaseHeld(smart.deck, smash ? beatsToSec(4) : smart.hard ? 0 : smart.blend);
            if (smash) dk.blendMode = mode;
        }
    }
    smart.peeking = false;
    smart.prep = false;
    smart.lastSwitch = t;
}
double barSeconds() { return 240.0 / std::max(30.0, tempo.bpm()); }
double beatsToSec(double beats) { return beats * barSeconds() / 4; }
int phraseBars() {
    double bars = S.interval / barSeconds();
    return bars >= 8 ? 8 : bars >= 4 ? 4 : bars >= 2 ? 2 : 1;
}

void smartTick(double t) {
    double beat = beatNow(), lead = 1.5 / barSeconds() * 4;  // ~1.5 s for the preset to compile
    auto events = structure.take();
    bool active = S.autoOn && S.smartMix && !locked;
    for (auto& e : events) {
        autoFxEvent(e.type);
        mappingEvent(e.type);
        if (e.type == MusicStructure::SongChange || e.type == MusicStructure::SongStart) vibe.set = false;  // a new song may bring its own colours
        smart.lastEvent = MusicStructure::name(e.type);
        smart.lastEventAt = t;
        if (!active) continue;
        double nextBar = structure.nextBoundary(beat + lead, 1);
        switch (e.type) {
            case MusicStructure::DropHit:
                if (smart.afterFake && !S.fakeoutRecut) { smart.afterFake = false; break; }  // keep the fake-drop preset
                smart.afterFake = false;
                if (smart.prep && smart.fireOn == 1 && smartReady()) smartFire(t);
                else if (smartPrepare(1, 0, beat, 0, true)) smart.fireBeat = beat;  // cut as soon as it compiles
                break;
            case MusicStructure::BreakdownStart: smartPrepare(-1, 0, nextBar, beatsToSec(16), false); break;  // long, soft
            case MusicStructure::SongStart:
                if (smart.prep && smart.fireOn == 2 && smartReady()) smartFire(t);
                else smartPrepare(0, 0, nextBar, beatsToSec(8), false);
                break;
            case MusicStructure::SongChange: smartPrepare(0, 0, nextBar, beatsToSec(8), false); break;
            case MusicStructure::SongEnd: smartPrepare(0, 2, 0, 0, true); break;
            case MusicStructure::Fakeout:  // that was not the real drop: get another one ready for it
                smart.afterFake = true;
                if (S.fakeoutRecut) smartPrepare(1, 1, 0, 0, true);
                break;
        }
    }
    if (!active) {
        if (smart.prep) smartCancel();
        return;
    }
    if (smart.prep && !smartValid()) smart.prep = false;  // failed to load, or the user took over the deck
    auto sec = structure.section();
    bool low = sec == MusicStructure::Breakdown || sec == MusicStructure::Build;
    if (smart.prep) {
        if (smart.fireOn == 0 && beat >= smart.fireBeat - 0.02) {
            if (smartReady()) smartFire(t);
            else smart.fireBeat = structure.nextBoundary(beat, 1);  // still compiling: next bar
        } else if (smart.fireOn == 1 && !low) {  // breakdown ended without a clear drop
            smart.fireOn = 0, smart.hard = false, smart.blend = beatsToSec(4);
            smart.fireBeat = structure.nextBoundary(beat, 1);
        } else if (smart.fireOn == 1 && sec == MusicStructure::Build && S.buildTease > 0) {
            // tension: the drop preset creeps in as the build-up rises, then snaps to full on the drop
            float p = (float)std::clamp(structure.sectionBeats(beat) / 32.0, 0.0, 1.0);
            if (p > 0.1f && !(afx.fakeBeat > afx.dropBeat && beat - afx.fakeBeat < 16)) smartPeek(S.buildTease * p * p);
        }
        return;
    }
    double since = t - std::max(smart.lastSwitch, decks[onAirDeck()].lastSwitch);
    if (low) {
        if (since > 2 && !(smart.afterFake && !S.fakeoutRecut)) smartPrepare(1, 1, 0, 0, true);  // have the drop preset ready
    } else if (sec != MusicStructure::Silence && since >= S.interval - 2 &&
               !(S.vocalEase && structure.vocalsOn() && since < S.interval * 2)) {  // let the singer finish the line
        // blend early so the new preset fully arrives on the phrase's first downbeat: two bars in a groove,
        // a quick beat during a drop (keeps the energy), one bar elsewhere
        double pre = sec == MusicStructure::Drop ? 1 : sec == MusicStructure::Groove ? 8 : 4;
        double land = structure.nextBoundary(beat + lead + pre, phraseBars());
        if (smartPrepare(0, 0, land - pre, beatsToSec(pre), false) && autoFxOn())
            afx.transition = pickTransition(sec == MusicStructure::Drop);
    }
}

void sortSimilar(std::vector<int>& v) {
    std::string refPath = S.pool.substr(8);
    auto ref = looks.find(refPath);
    if (ref == looks.end()) return;
    const Look r = ref->second;
    std::vector<std::pair<float, int>> d;
    for (int i : v) {
        auto it = looks.find(lib[i].path);
        if (it != looks.end()) d.push_back({lib[i].path == refPath ? -1.f : lookDistance(r, it->second), i});
    }
    std::sort(d.begin(), d.end());
    v.clear();
    for (size_t k = 0; k < d.size() && k < 80; k++) v.push_back(d[k].second);
}

