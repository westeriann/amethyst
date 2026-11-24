// Song-structure follower for the auto-mixer: detects song start/end/change, breakdowns, builds and
// drops from the audio, and keeps a bar/phrase grid so switches land on downbeats.
#pragma once
#include <cstddef>
#include <vector>

class MusicStructure {
public:
    enum Section { Silence, Intro, Groove, Breakdown, Build, Drop };
    enum Event { SongStart, SongEnd, SongChange, BreakdownStart, DropHit, Fakeout };
    struct Ev {
        Event type;
        double beat, time;
    };

    // 48 kHz interleaved stereo. `beat` is the beat-clock position at time `t` (start of the block).
    void feed(const float* stereo, size_t frames, double t, double beat, double bpm);
    void trackChanged(double t, double beat);  // e.g. DJ software reported a new title
    std::vector<Ev> take();

    Section section() const { return sec_; }
    static const char* name(Section s);
    static const char* name(Event e);
    // Bar grid. Phrases are 8 bars, anchored on the last detected drop (or the beat clock's own bar 1).
    int beatInBar(double beat) const;
    int barInPhrase(double beat) const;
    double nextBoundary(double beat, int bars) const;  // next beat that starts a block of `bars` bars
    double sectionBeats(double beat) const { return beat - secBeat_; }
    float energy() const;  // bass relative to the song's full level, 0..~1.5
    // Vocals: a single pitched line in the voice range that glides/wobbles. 0..1, smoothed.
    float vocal() const { return vocalAmt_; }
    bool vocalsOn() const { return vocalOn_; }
    float pitchClarity() const { return clarity_; }
    // Percussion: hi-hats/shakers (above ~7 kHz) and snares/claps (~1-4 kHz, not riding on a kick).
    // hat()/snare() are 0..1 hit envelopes; the hit counters let callers react once per hit.
    float hat() const { return hatEnv_; }
    float snare() const { return snEnv_; }
    long long hatHits() const { return hatHits_; }
    long long snareHits() const { return snHits_; }
    float hatsPerBeat() const { return hatRate_; }  // smoothed, e.g. ~1 for offbeat house hats, 2-4 for 8ths/16ths
    // tests: hold a section regardless of the audio (pin = false releases it)
    void debugPin(Section s, double beat, bool pin = true) { enter(s, beat); pinned_ = pin; }
    void debugEmit(Event e, double beat, double t) { emit(e, beat, t); }

private:
    Section sec_ = Silence;
    bool pinned_ = false;
    double secBeat_ = 0, anchor_ = 0;
    bool anchored_ = false;
    std::vector<Ev> out_;
    // signal
    float lp_ = 0, bassF_ = 0, bassM_ = 0, lvlM_ = 0, bassRef_ = 0, lowLvlMin_ = 1;
    double silentFor_ = 0, soundFor_ = 0, lowSince_ = -1, songBeat_ = 0;
    // tempo
    double stableBpm_ = 0, bpmSince_ = 0, offBpmFor_ = 0;
    float ringPeak_[32] = {};  // per-beat bass peaks
    long long ringIdx_[32] = {};
    double candBpm_ = 0, candFor_ = 0;
    // fakeout tracking
    long long lastIb_ = 0, lowRunStart_ = 0, winStart_ = -(1LL << 40);
    int lowRun_ = 0;
    float dropLevel_ = 0;
    bool fakeUsed_ = false;  // one fakeout per drop: stop-start bass after it is the drop's rhythm
    float peakOf(long long b) const;
    // vocal detector (works on a 200 Hz - 3.5 kHz band, downsampled to 12 kHz)
    float vhp_ = 0, vhpIn_ = 0, vlp_ = 0, vAcc_ = 0, clarity_ = 0, vocalAmt_ = 0, lastPitch_ = 0;
    int vN_ = 0, vHead_ = 0, vSince_ = 0;
    float vRing_[1024] = {};
    bool vocalOn_ = false;
    void vocalStep();
    // percussion detector
    float h1_ = 0, h1In_ = 0, h2_ = 0, h2In_ = 0, s1_ = 0, s1In_ = 0, s2_ = 0, s2In_ = 0, sLp_ = 0;
    double accH_ = 0, accS_ = 0;
    float hatM_ = 0, snM_ = 0, hatPrev_ = 0, snPrev_ = 0, bassPrevHop_ = 0, hatEnv_ = 0, snEnv_ = 0, hatRate_ = 0;
    double hatGap_ = 1, snGap_ = 1;
    long long hatHits_ = 0, snHits_ = 0, rateBeat_ = 0, rateBase_ = 0;
    void percStep(float hat, float sn, float bass, float lvl, double beat, double dt);
    // hop accumulator
    double accB_ = 0, accA_ = 0;
    int accN_ = 0;
    void hop(float bass, float lvl, double t, double beat, double bpm, double dt);
    void enter(Section s, double beat);
    void emit(Event e, double beat, double t) { out_.push_back({e, beat, t}); }
};
