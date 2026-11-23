#include "tempo.h"

#include <ableton/Link.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
constexpr int kHop = 480;           // 10 ms at 48 kHz -> 100 Hz onset envelope
constexpr double kHopSec = 0.01;
constexpr int kHistory = 1024;      // ~10 s of envelope
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
    double hopEnergy = 0, lp = 0, prevLog = 0;
    double lastEstimate = 0;
    double pendingBpm = 0;
    int pendingVotes = 0;

    std::vector<double> taps;

    std::unique_ptr<ableton::Link> link;

    double beatAt(double t) const { return anchorBeat + (t - anchorTime) * bpm / 60.0; }
    void setTempoKeepPhase(double newBpm, double t) {
        anchorBeat = beatAt(t);
        anchorTime = t;
        bpm = newBpm;
    }
    float odAt(int back) const { return od[(head - 1 - back + kHistory * 4) % kHistory]; }

    void estimate(double now) {
        if (filled < 500) return;
        const int n = std::min(filled, kHistory);
        std::vector<float> x(n);
        double mean = 0;
        for (int i = 0; i < n; i++) { x[i] = odAt(n - 1 - i); mean += x[i]; }
        mean /= n;
        for (auto& v : x) v = (float)(v - mean);
        const int minLag = 33, maxLag = 92;  // ~182 .. 65 BPM
        std::vector<double> score(maxLag + 2, 0.0);
        double best = -1e9, sum = 0;
        int bestLag = 50;
        for (int L = minLag; L <= maxLag + 1; L++) {
            double s = 0;
            for (int i = L; i < n; i++) s += x[i] * x[i - L];
            // also reward the double period (bars feel stronger than beats)
            if (2 * L < n) for (int i = 2 * L; i < n; i += 2) s += 0.5 * x[i] * x[i - 2 * L];
            score[L] = s;
        }
        for (int L = minLag; L <= maxLag; L++) {
            double w = std::exp(-0.5 * std::pow(std::log2(L / 50.0) / 0.9, 2));  // prefer ~120 BPM
            double s = score[L] * w;
            sum += std::max(0.0, s);
            if (s > best) { best = s; bestLag = L; }
        }
        double avg = sum / (maxLag - minLag + 1);
        confidence = (float)(avg > 0 ? std::min(10.0, best / avg) : 0);
        if (best <= 0 || confidence < 1.6) return;
        // parabolic refinement
        double a = score[bestLag - 1], b = score[bestLag], c = score[bestLag + 1];
        double denom = a - 2 * b + c;
        double lag = bestLag + (std::fabs(denom) > 1e-9 ? 0.5 * (a - c) / denom : 0);
        double found = 60.0 / (lag * kHopSec);

        if (std::fabs(found - bpm) / bpm < 0.04) {
            setTempoKeepPhase(bpm * 0.85 + found * 0.15, now);
        } else if (pendingBpm > 0 && std::fabs(found - pendingBpm) / pendingBpm < 0.04) {
            if (++pendingVotes >= 3) { setTempoKeepPhase(found, now); pendingVotes = 0; pendingBpm = 0; }
        } else {
            pendingBpm = found;
            pendingVotes = 1;
        }

        // phase: which offset inside one period lines up with the onsets?
        const double period = 60.0 / bpm / kHopSec;
        int bestOff = 0;
        double bestSum = -1;
        for (int o = 0; o < (int)period; o++) {
            double s = 0;
            for (double k = o; k < n; k += period) s += odAt((int)k);
            if (s > bestSum) { bestSum = s; bestOff = o; }
        }
        double lastBeatTime = lastHopTime - bestOff * kHopSec;
        double cur = beatAt(lastBeatTime);
        double err = cur - std::round(cur);
        anchorBeat -= err * 0.5;  // ease the phase towards the detected beat
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
        I.lp += (m - I.lp) * 0.025;
        I.hopEnergy += I.lp * I.lp + 0.15 * m * m;
        if (++I.hopCount >= kHop) {
            double lg = std::log10(1e-7 + I.hopEnergy / kHop);
            float v = (float)std::max(0.0, lg - I.prevLog);
            I.prevLog = lg;
            I.od[I.head] = v;
            I.head = (I.head + 1) % kHistory;
            I.filled = std::min(I.filled + 1, kHistory);
            I.hopEnergy = 0;
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
