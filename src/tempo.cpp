#include "tempo.h"

#include <ableton/Link.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
constexpr int kHop = 480;           // 10 ms at 48 kHz -> 100 Hz onset envelope
constexpr double kHopSec = 0.01;
constexpr int kHistory = 1500;      // 15 s of onset envelope
constexpr double kMinBpm = 70, kStep = 0.25;
constexpr int kBins = (int)((190 - kMinBpm) / kStep) + 1;  // 70..190 BPM
constexpr double kQuantum = 4.0;    // beats per bar for Link phase
}  // namespace

struct Tempo::Impl {
    Source source = Source::Auto;
    double bpm = 120, anchorTime = 0, anchorBeat = 0;
    float confidence = 0;

    // onset envelope
    std::vector<float> od = std::vector<float>(kHistory, 0.f);
    int head = 0, filled = 0;
    double lastHopTime = 0;
    int hopCount = 0;
    double eLow = 0, eMid = 0, eHigh = 0, lpLow = 0, lpMid = 0, lpHigh = 0, prevL[3] = {0, 0, 0};
    double lastEstimate = 0;
    double pendingBpm = 0;
    int pendingVotes = 0;
    bool locked = false;
    double strongAvg = 0;  // typical onset strength of this song
    std::vector<double> acc;  // accumulated tempo scores

    std::vector<double> taps;

    std::unique_ptr<ableton::Link> link;

    double beatAt(double t) const { return anchorBeat + (t - anchorTime) * bpm / 60.0; }
    void setTempoKeepPhase(double newBpm, double t) {
        anchorBeat = beatAt(t);
        anchorTime = t;
        bpm = newBpm;
    }
    float odAt(int back) const { return od[(head - 1 - back + kHistory * 4) % kHistory]; }

    // Tempo: autocorrelate the onset envelope, score every candidate BPM by how well its 1..4 beat lags
    // (and the half-beat) line up, accumulate the scores over ~8 s and only switch on sustained evidence.
    void estimate(double now) {
        if (filled < 400) return;
        const int n = std::min(filled, kHistory);
        std::vector<float> x(n);
        double mean = 0;
        for (int i = 0; i < n; i++) { x[i] = odAt(n - 1 - i); mean += x[i]; }
        mean /= n;
        // how strong the rhythm is right now vs. this song so far: drumless breakdowns barely move the lock
        double recent = 0;
        for (int i = std::max(0, n - 300); i < n; i++) recent += x[i];
        recent /= std::min(n, 300);
        strongAvg = strongAvg <= 0 ? recent : strongAvg + (recent - strongAvg) * 0.02;
        double w = std::clamp(recent / std::max(1e-6, strongAvg), 0.05, 1.5);
        for (auto& v : x) v = (float)(v - mean);
        const int maxL = std::min(n - 50, 360);
        std::vector<double> acf(maxL + 2, 0.0);
        for (int L = 1; L <= maxL + 1 && L < n; L++) {
            double s = 0;
            for (int i = L; i < n; i++) s += x[i] * x[i - L];
            acf[L] = s / (n - L);
        }
        auto A = [&](double L) {
            int a = (int)L;
            if (a < 1 || a + 1 > maxL + 1) return 0.0;
            double k = L - a;
            return acf[a] * (1 - k) + acf[a + 1] * k;
        };
        if (acc.empty()) acc.assign(kBins, 0.0);
        std::vector<double> sc(kBins);
        double mx = 1e-12;
        for (int i = 0; i < kBins; i++) {
            double b = kMinBpm + i * kStep, P = 6000.0 / b;
            double s = A(P) + 0.8 * A(2 * P) + 0.6 * A(3 * P) + 0.6 * A(4 * P) + 0.5 * A(P / 2) - 0.3 * A(P * 1.5) - 0.4 * A(P * 2 / 3) - 0.3 * A(P / 3) - 0.25 * A(P * 4 / 3);
            sc[i] = std::max(0.0, s);
            mx = std::max(mx, sc[i]);
        }
        double sum = 0;
        int best = 0;
        for (int i = 0; i < kBins; i++) {
            acc[i] = acc[i] * (1 - 0.04 * w) + w * sc[i] / mx;
            sum += acc[i];
        }
        auto prior = [](double b) { double d = std::log2(b / 140.0); return std::exp(-0.5 * d * d / (0.6 * 0.6)); };
        double bestV = -1;
        for (int i = 0; i < kBins; i++) {
            double v = acc[i] * prior(kMinBpm + i * kStep);
            if (v > bestV) { bestV = v; best = i; }
        }
        // refine
        double off = 0;
        if (best > 0 && best < kBins - 1) {
            double a = acc[best - 1], b = acc[best], c = acc[best + 1], d = a - 2 * b + c;
            if (std::fabs(d) > 1e-12) off = std::clamp(0.5 * (a - c) / d, -0.5, 0.5);
        }
        double found = kMinBpm + (best + off) * kStep;
        double avg = sum / kBins;
        confidence = (float)(avg > 0 ? std::min(10.0, acc[best] / avg) : 0);
        if (confidence < 1.5) return;
        auto valueAt = [&](double b) {
            int i = (int)std::lround((b - kMinBpm) / kStep);
            return i >= 0 && i < kBins ? acc[i] * prior(b) : 0.0;
        };
        // triplet-heavy music makes 4:3, 3:2 and 2:1 neighbours of the real tempo look good for a while:
        // once locked, those need far stronger and longer evidence than an unrelated tempo
        auto related = [&](double f) {
            for (double q : {4.0 / 3, 3.0 / 4, 3.0 / 2, 2.0 / 3, 2.0, 0.5})
                if (std::fabs(f / bpm / q - 1) < 0.03) return true;
            return false;
        };
        if (std::fabs(found - bpm) / bpm < 0.02) {
            setTempoKeepPhase(bpm + (found - bpm) * 0.3, now);
            pendingVotes = 0;
        } else if (!locked || valueAt(found) > valueAt(bpm) * (related(found) ? 2.0 : 1.3)) {
            if (pendingBpm > 0 && std::fabs(found - pendingBpm) / pendingBpm < 0.02) {
                if (++pendingVotes >= (!locked ? 2 : related(found) ? 24 : 10)) { setTempoKeepPhase(found, now); pendingVotes = 0; pendingBpm = 0; locked = true; }
            } else {
                pendingBpm = found;
                pendingVotes = 1;
            }
        }

        // phase: comb over the last ~6 s, recent onsets weigh more
        const double period = 6000.0 / bpm;
        const int span = std::min(n, 600);
        double bestSum = -1, bestOff = 0;
        for (double o = 0; o < period; o += 0.5) {
            double s = 0, w = 1;
            for (double k = o; k < span - 1; k += period, w *= 0.93) {
                int a = (int)k;
                double f = k - a;
                s += w * (odAt(a) * (1 - f) + odAt(a + 1) * f);
            }
            if (s > bestSum) { bestSum = s; bestOff = o; }
        }
        double lastBeatTime = lastHopTime - bestOff * kHopSec;
        double cur = beatAt(lastBeatTime);
        double err = cur - std::round(cur);
        anchorBeat -= err * 0.35;  // ease the phase towards the detected beat
    }
};

Tempo::Tempo() : p_(new Impl) {}
Tempo::~Tempo() {
    if (p_->link) p_->link->enable(false);
    delete p_;
}

void Tempo::feed(const float* s, size_t frames, double now) {
    Impl& I = *p_;
    const size_t totalHops = (I.hopCount + frames) / kHop;
    size_t hopIndex = 0;
    for (size_t i = 0; i < frames; i++) {
        float m = 0.5f * (s[2 * i] + s[2 * i + 1]);
        I.lpLow += (m - I.lpLow) * 0.0194;   // ~150 Hz
        I.lpMid += (m - I.lpMid) * 0.23;     // ~2 kHz
        I.lpHigh += (m - I.lpHigh) * 0.48;   // ~5 kHz
        double lo = I.lpLow, mi = I.lpMid - I.lpLow, hi = m - I.lpHigh;
        I.eLow += lo * lo, I.eMid += mi * mi, I.eHigh += hi * hi;
        if (++I.hopCount >= kHop) {
            // band-wise log-energy flux (rises only): kicks, snares and hats all count
            double e[3] = {I.eLow, I.eMid, I.eHigh}, w[3] = {1.0, 0.8, 0.6}, v = 0;
            for (int b = 0; b < 3; b++) {
                double lg = std::log10(1e-9 + e[b] / kHop);
                v += w[b] * std::max(0.0, lg - I.prevL[b]);
                I.prevL[b] = lg;
            }
            I.eLow = I.eMid = I.eHigh = 0;
            I.od[I.head] = (float)v;
            I.head = (I.head + 1) % kHistory;
            I.filled = std::min(I.filled + 1, kHistory);
            I.hopCount = 0;
            hopIndex++;
            I.lastHopTime = now - (double)(totalHops - hopIndex) * kHopSec;
        }
    }
}

void Tempo::update(double now) {
    if (p_->source == Source::Auto && now - p_->lastEstimate > 0.5) {
        p_->lastEstimate = now;
        p_->estimate(now);
    }
}

void Tempo::tap(double now) {
    Impl& I = *p_;
    if (!I.taps.empty() && now - I.taps.back() > 2.0) I.taps.clear();
    I.taps.push_back(now);
    if (I.taps.size() > 5) I.taps.erase(I.taps.begin());
    if (I.source == Source::Auto) I.source = Source::Tap;
    if (I.taps.size() >= 2) {
        double avg = (I.taps.back() - I.taps.front()) / (I.taps.size() - 1);
        double b = std::clamp(60.0 / avg, 40.0, 240.0);
        setBpm(b, now);
    }
    // the tap is a beat: snap phase
    double cur = I.beatAt(now);
    I.anchorBeat = std::round(cur);
    I.anchorTime = now;
    if (I.source == Source::Link && I.link) {
        auto st = I.link->captureAppSessionState();
        auto t = I.link->clock().micros();
        st.requestBeatAtTime(std::round(st.beatAtTime(t, kQuantum)), t, kQuantum);
        I.link->commitAppSessionState(st);
    }
}

void Tempo::setBpm(double b, double now) {
    Impl& I = *p_;
    I.setTempoKeepPhase(std::clamp(b, 20.0, 300.0), now);
    if (I.link && I.source == Source::Link) {
        auto st = I.link->captureAppSessionState();
        st.setTempo(I.bpm, I.link->clock().micros());
        I.link->commitAppSessionState(st);
    }
}

void Tempo::nudge(double beats) { p_->anchorBeat += beats; }

void Tempo::setExternal(double b, double beat, double now) {
    Impl& I = *p_;
    if (b <= 0) return;
    I.source = Source::External;
    I.bpm = b;
    I.anchorBeat = beat;
    I.anchorTime = now;
}

double Tempo::bpm() const {
    if (p_->source == Source::Link && p_->link && p_->link->isEnabled()) return p_->link->captureAppSessionState().tempo();
    return p_->bpm;
}

double Tempo::beatAt(double now) const {
    if (p_->source == Source::Link && p_->link && p_->link->isEnabled()) {
        auto st = p_->link->captureAppSessionState();
        return st.beatAtTime(p_->link->clock().micros(), kQuantum);
    }
    return p_->beatAt(now);
}

float Tempo::confidence() const { return p_->confidence; }

void Tempo::setSource(Source s) {
    p_->source = s;
    if (s == Source::Link) enableLink(true);
}
Tempo::Source Tempo::source() const { return p_->source; }
const char* Tempo::sourceName() const {
    switch (p_->source) {
        case Source::Auto: return "Auto";
        case Source::Tap: return "Tap";
        case Source::External: return "DJ";
        default: return "Link";
    }
}

void Tempo::enableLink(bool on) {
    if (on && !p_->link) p_->link = std::make_unique<ableton::Link>(p_->bpm);
    if (p_->link) p_->link->enable(on);
    if (!on && p_->source == Source::Link) p_->source = Source::Auto;
}
bool Tempo::linkEnabled() const { return p_->link && p_->link->isEnabled(); }
int Tempo::linkPeers() const { return p_->link ? (int)p_->link->numPeers() : 0; }

void Tempo::restart(double bpmHint) {
    Impl& I = *p_;
    std::fill(I.od.begin(), I.od.end(), 0.f);
    I.head = I.filled = 0;
    I.acc.clear();
    I.pendingBpm = 0, I.pendingVotes = 0, I.confidence = 0;
    I.eLow = I.eMid = I.eHigh = 0, I.hopCount = 0;
    I.lastEstimate = 0;
    I.anchorBeat = 0, I.anchorTime = 0;
    I.locked = bpmHint > 0;
    I.strongAvg = 0;
    I.bpm = bpmHint > 0 ? bpmHint : 120;
}
