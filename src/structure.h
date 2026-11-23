// Song-structure follower for the auto-mixer: detects song start/end/change, breakdowns, builds and
// drops from the audio, and keeps a bar/phrase grid so switches land on downbeats.
#pragma once
#include <cstddef>
#include <vector>

class MusicStructure {
public:
    enum Section { Silence, Intro, Groove, Breakdown, Build, Drop };
    enum Event { SongStart, SongEnd, SongChange, BreakdownStart, DropHit };
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

private:
    Section sec_ = Silence;
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
    // hop accumulator
    double accB_ = 0, accA_ = 0;
    int accN_ = 0;
    void hop(float bass, float lvl, double t, double beat, double bpm, double dt);
    void enter(Section s, double beat);
    void emit(Event e, double beat, double t) { out_.push_back({e, beat, t}); }
};
