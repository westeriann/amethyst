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

int smartPick(int energy) {
    AutoPickScope scope;
    int on = onAirDeck();
    if (energy == 0) return S.shuffle ? pickRandom(decks[on].cur) : stepFrom(decks[on].cur, 1);
    std::vector<int> c;
    for (int i : pickPool()) {
        if (lib[i].broken || i == decks[0].cur || i == decks[1].cur) continue;
        auto it = looks.find(lib[i].path);
        if (it == looks.end()) continue;
        if (energyClass(it->second) == energy) c.push_back(i);
    }
    if (c.empty()) return pickRandom(decks[on].cur);
    return c[std::uniform_int_distribution<size_t>(0, c.size() - 1)(rng)];
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
            afx.transition = sec == MusicStructure::Drop ? (rng() % 2 ? TransZoom : TransGlitch) : smoothTransition();
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

