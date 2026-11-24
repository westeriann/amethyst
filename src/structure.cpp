#include "structure.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kHop = 512;
constexpr double kRate = 48000.0;
constexpr float kQuiet = 0.003f;  // about -50 dBFS
}  // namespace

static double posMod(double a, double m) {
    double r = std::fmod(a, m);
    return r < 0 ? r + m : r;
}

const char* MusicStructure::name(Section s) {
    static const char* n[] = {"Silence", "Intro", "Groove", "Breakdown", "Build-up", "Drop"};
    return n[s];
}
const char* MusicStructure::name(Event e) {
    static const char* n[] = {"Song start", "Song end", "New song", "Breakdown", "Drop", "Fakeout"};
    return n[e];
}

float MusicStructure::peakOf(long long b) const {
    int k = (int)posMod((double)b, 32);
    return ringIdx_[k] == b ? ringPeak_[k] : -1.f;
}

void MusicStructure::enter(Section s, double beat) {
    sec_ = s;
    secBeat_ = beat;
}

std::vector<MusicStructure::Ev> MusicStructure::take() {
    std::vector<Ev> v;
    v.swap(out_);
    return v;
}

int MusicStructure::beatInBar(double beat) const { return (int)posMod(std::floor(beat - anchor_ + 1e-6), 4); }
int MusicStructure::barInPhrase(double beat) const { return (int)posMod(std::floor((beat - anchor_) / 4 + 1e-6), 8); }
double MusicStructure::nextBoundary(double beat, int bars) const {
    double u = std::max(1, bars) * 4.0;
    return anchor_ + (std::floor((beat - anchor_) / u + 1e-6) + 1) * u;
}
float MusicStructure::energy() const { return bassRef_ > 1e-4f ? bassM_ / bassRef_ : 0.f; }

// Pitch clarity by normalised autocorrelation over 85 ms (100..800 Hz). Voices are clear AND their
// pitch keeps moving a little (glides, vibrato); synth leads tend to hold dead-steady notes.
void MusicStructure::vocalStep() {
    const int N = 1024, W = 768;
    float x[1024];
    double e = 0;
    for (int i = 0; i < N; i++) x[i] = vRing_[(vHead_ + i) & 1023], e += x[i] * x[i];
    float clar = 0;
    int bestL = 0;
    if (e / N > 1e-7) {
        double e0 = 0;
        for (int i = 0; i < W; i++) e0 += x[i] * x[i];
        for (int L = 15; L <= 120; L++) {
            double r = 0, eL = 0;
            for (int i = 0; i < W; i++) r += x[i] * x[i + L], eL += x[i + L] * x[i + L];
            float v = (float)(r / std::sqrt(e0 * eL + 1e-12));
            if (v > clar) clar = v, bestL = L;
        }
    }
    clarity_ += (clar - clarity_) * 0.3f;
    float pitch = bestL ? 12000.f / bestL : 0;
    float score = 0;
    if (clar > 0.55f && pitch > 0) {
        float ch = lastPitch_ > 0 ? std::fabs(pitch / lastPitch_ - 1) : 1;
        score = ch > 0.004f && ch < 0.08f ? 1.f : ch <= 0.004f ? 0.35f : 0.5f;  // moving a little = voice-like
    }
    lastPitch_ = clar > 0.55f ? pitch : 0;
    vocalAmt_ += (score - vocalAmt_) * 0.06f;  // ~0.7 s smoothing
    if (!vocalOn_ && vocalAmt_ > 0.45f) vocalOn_ = true;
    else if (vocalOn_ && vocalAmt_ < 0.25f) vocalOn_ = false;
}

// Hits are sudden jumps of a band's level over its own recent average. A snare band jump that comes
// with a kick (bass jumping at the same time) is the kick's click, not a snare.
void MusicStructure::percStep(float hat, float sn, float bass, float lvl, double beat, double dt) {
    auto ema = [dt](float& st, float v, double tau) { st += (v - st) * (float)(1 - std::exp(-dt / tau)); };
    hatEnv_ *= (float)std::exp(-dt / 0.08);
    snEnv_ *= (float)std::exp(-dt / 0.15);
    hatGap_ += dt, snGap_ += dt;
    bool loud = lvl >= kQuiet;
    bool kick = bass > 0.002f && bass > bassPrevHop_ * 1.35f;
    if (loud && hatGap_ >= 0.06 && hat > hatM_ * 1.5f && hat > hatPrev_ * 1.2f && hat > lvl * 0.008f) {
        hatEnv_ = std::max(hatEnv_, std::clamp((hat / (hatM_ + 1e-6f) - 1) / 1.5f, 0.3f, 1.f));
        hatHits_++;
        hatGap_ = 0;
    }
    if (loud && !kick && snGap_ >= 0.12 && sn > snM_ * 1.8f && sn > snPrev_ * 1.3f && sn > lvl * 0.05f) {
        snEnv_ = std::max(snEnv_, std::clamp((sn / (snM_ + 1e-6f) - 1) / 2.f, 0.3f, 1.f));
        snHits_++;
        snGap_ = 0;
    }
    ema(hatM_, hat, 0.4);
    ema(snM_, sn, 0.4);
    hatPrev_ = hat, snPrev_ = sn, bassPrevHop_ = bass;
    long long ib = (long long)std::floor(beat);
    if (ib != rateBeat_) {
        if (ib > rateBeat_ && ib - rateBeat_ < 8) hatRate_ += ((float)(hatHits_ - rateBase_) / (float)(ib - rateBeat_) - hatRate_) * 0.12f;
        rateBeat_ = ib, rateBase_ = hatHits_;
    }
}

void MusicStructure::trackChanged(double t, double beat) {
    emit(SongChange, beat, t);
    anchored_ = false;
    stableBpm_ = 0;
    songBeat_ = beat;
    if (sec_ == Silence) enter(Intro, beat);
}

void MusicStructure::feed(const float* s, size_t frames, double t, double beat, double bpm) {
    if (pinned_) return;
    const float a = 1.f - std::exp(-2.f * 3.14159265f * 150.f / (float)kRate);  // ~150 Hz low-pass for the kick
    double bps = std::max(30.0, bpm) / 60.0;
    for (size_t i = 0; i < frames; i++) {
        float m = 0.5f * (s[i * 2] + s[i * 2 + 1]);
        lp_ += a * (m - lp_);
        accB_ += lp_ * lp_;
        accA_ += m * m;
        // vocal band: one-pole high-pass ~200 Hz, low-pass ~3.5 kHz, then 4:1 down to 12 kHz
        vhp_ = 0.974f * (vhp_ + m - vhpIn_);
        vhpIn_ = m;
        vlp_ += 0.37f * (vhp_ - vlp_);
        vAcc_ += vlp_;
        // percussion bands: hats = two one-pole high-passes at ~7 kHz; snares = 2x high-pass ~1 kHz + low-pass ~4 kHz
        h1_ = 0.40f * (h1_ + m - h1In_), h1In_ = m;
        h2_ = 0.40f * (h2_ + h1_ - h2In_), h2In_ = h1_;
        accH_ += h2_ * h2_;
        s1_ = 0.877f * (s1_ + m - s1In_), s1In_ = m;
        s2_ = 0.877f * (s2_ + s1_ - s2In_), s2In_ = s1_;
        sLp_ += 0.408f * (s2_ - sLp_);
        accS_ += sLp_ * sLp_;
        if (++vN_ == 4) {
            vRing_[vHead_] = vAcc_ * 0.25f;
            vHead_ = (vHead_ + 1) & 1023;
            vAcc_ = 0, vN_ = 0;
            if (++vSince_ >= 512) vSince_ = 0, vocalStep();  // ~23 times a second
        }
        if (++accN_ == kHop) {
            double dt = kHop / kRate, off = (i + 1) / kRate;
            float bassRms = (float)std::sqrt(accB_ / kHop), lvlRms = (float)std::sqrt(accA_ / kHop);
            percStep((float)std::sqrt(accH_ / kHop), (float)std::sqrt(accS_ / kHop), bassRms, lvlRms, beat + off * bps, dt);
            hop(bassRms, lvlRms, t + off, beat + off * bps, bpm, dt);
            accB_ = accA_ = accH_ = accS_ = 0;
            accN_ = 0;
        }
    }
}

void MusicStructure::hop(float bass, float lvl, double t, double beat, double bpm, double dt) {
    auto ema = [dt](float& st, float v, double tau) { st += (v - st) * (float)(1 - std::exp(-dt / tau)); };
    ema(bassF_, bass, 0.05);
    ema(bassM_, bass, 0.8);
    ema(lvlM_, lvl, 0.8);
    if (lvl < kQuiet) silentFor_ += dt, soundFor_ = 0;
    else soundFor_ += dt, silentFor_ = 0;

    // per-beat bass peaks
    long long ib = (long long)std::floor(beat);
    int slot = (int)posMod((double)ib, 32);
    if (ringIdx_[slot] != ib) ringIdx_[slot] = ib, ringPeak_[slot] = 0;
    ringPeak_[slot] = std::max(ringPeak_[slot], bassF_);

    if (sec_ == Silence) {
        if (soundFor_ >= 0.2) {
            emit(SongStart, beat, t);
            fakeUsed_ = false;
            enter(Intro, beat);
            bassRef_ = bassM_;
            stableBpm_ = 0;
            anchored_ = false;
            lowSince_ = -1;
            songBeat_ = beat;
        }
        return;
    }
    if (silentFor_ >= 1.2) {
        emit(SongEnd, beat, t);
        enter(Silence, beat);
        return;
    }

    bool low = sec_ == Breakdown || sec_ == Build;
    // the song's "full" bass level: rises quickly, decays slowly, frozen during breakdowns
    if (bassM_ > bassRef_) ema(bassRef_, bassM_, 1.5);
    else if (!low) ema(bassRef_, bassM_, 40);

    // new song from a sustained tempo change (ignore half/double-time flips)
    if (bpm > 0) {
        if (stableBpm_ <= 0) {
            // trust the tempo only once it has held for 45 s
            if (candBpm_ > 0 && std::fabs(bpm / candBpm_ - 1) < 0.02) candFor_ += dt;
            else candBpm_ = bpm, candFor_ = 0;
            if (candFor_ >= 45) stableBpm_ = candBpm_;
        } else {
            double r = bpm / stableBpm_;
            bool off = std::fabs(r - 1) > 0.06;
            for (double q : {2.0, 0.5, 1.5, 2.0 / 3, 4.0 / 3, 0.75})  // beat-clock octave/triplet slips are not new songs
                if (std::fabs(r / q - 1) < 0.04) off = false;
            if (off) {
                if ((offBpmFor_ += dt) >= 8) {
                    emit(SongChange, beat, t);
                    stableBpm_ = 0;
                    candFor_ = 0;
                    songBeat_ = beat;
                    offBpmFor_ = 0;
                    anchored_ = false;
                }
            } else {
                offBpmFor_ = 0;
                stableBpm_ += (bpm - stableBpm_) * 0.001;
            }
        }
    }

    // fakeout: a "drop" whose bass disappears again for most of a bar within its first three bars
    if (sec_ == Drop && ib != lastIb_ && !fakeUsed_) {
        long long D = std::llround(secBeat_), done = ib - 1;  // last completed beat
        float pk = peakOf(done);
        if (done <= D + 1) {
            dropLevel_ = std::max(dropLevel_, pk);
        } else if (done <= D + 18 && pk >= 0) {
            if (pk < dropLevel_ * 0.3f) {
                if (lowRun_++ == 0) lowRunStart_ = done;
            } else {
                lowRun_ = 0;
            }
            if (lowRun_ >= 3 && lowRunStart_ <= D + 16) {  // fake drops of up to four bars
                emit(Fakeout, (double)lowRunStart_, t);
                fakeUsed_ = true;
                enter(Build, (double)lowRunStart_);
                winStart_ = lowRunStart_;  // the real drop is measured against the quiet part only
                lowRun_ = 0;
            }
        }
    }
    lastIb_ = ib;

    // drop: the bass comes back far above everything in the previous two bars
    if (beat - songBeat_ >= 16 && (sec_ != Drop || beat - secBeat_ >= 32)) {
        float prev = 0;
        bool full = true;
        long long from = std::max(ib - 8, winStart_);
        if (ib - from < 2) full = false;
        for (long long b = from; full && b < ib; b++) {  // up to two bars without real bass before it
            int k = (int)posMod((double)b, 32);
            if (ringIdx_[k] != b) { full = false; break; }
            prev = std::max(prev, ringPeak_[k]);
        }
        if (full && bassF_ > 0.004f && bassF_ > prev * 1.8f && bassF_ > bassRef_ * 0.5f) {
            double at = std::round(beat);
            emit(DropHit, at, t);
            anchor_ = at;
            anchored_ = true;
            enter(Drop, at);
            lowSince_ = -1;
            dropLevel_ = bassF_;
            lowRun_ = 0;
            winStart_ = -(1LL << 40);
            return;
        }
    }

    // breakdown: bass stays well under the song's level for a bar
    if (!low) {
        if (bassM_ < bassRef_ * 0.35f && lvl >= kQuiet) {
            if (lowSince_ < 0) lowSince_ = beat;
            if (beat - lowSince_ >= 8 && sec_ != Intro) {  // two bars: short gaps and fills are not breakdowns
                // the bass actually stopped on the first quiet beat (the smoothed level lags behind)
                long long st = (long long)std::floor(lowSince_);
                for (long long b = st; b > ib - 12; b--) {
                    int k = (int)posMod((double)b, 32);
                    if (ringIdx_[k] != b || ringPeak_[k] >= bassRef_ * 0.5f) break;
                    st = b;
                }
                double at = (double)st;
                emit(BreakdownStart, at, t);
                fakeUsed_ = false;
                if (!anchored_) anchor_ = at;
                enter(Breakdown, at);
                lowLvlMin_ = lvlM_;
            }
        } else if (bassM_ > bassRef_ * 0.45f) {
            lowSince_ = -1;
        }
        if (sec_ == Intro && beat - secBeat_ >= 16 && bassRef_ > 0.004f && bassM_ > bassRef_ * 0.6f) enter(Groove, beat);
        if (sec_ == Drop && beat - secBeat_ >= 64) enter(Groove, beat), fakeUsed_ = false;
    } else {
        lowLvlMin_ = std::min(lowLvlMin_, lvlM_);
        if (sec_ == Breakdown && beat - secBeat_ >= 8 && lvlM_ > lowLvlMin_ * 1.4f) enter(Build, beat);
        if (bassM_ > bassRef_ * 0.6f && beat - secBeat_ >= 8) enter(Groove, beat);  // bass faded back in without a clear drop
    }
}
