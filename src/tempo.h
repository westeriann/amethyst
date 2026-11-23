// Beat clock: automatic BPM + phase detection from the audio, tap tempo, or Ableton Link.
#pragma once
#include <cstddef>

class Tempo {
public:
    enum class Source { Auto, Tap, Link, External };  // External = DJ gear (StageLinQ)

    Tempo();
    ~Tempo();

    void feed(const float* stereo, size_t frames, double now);  // 48 kHz interleaved stereo
    void update(double now);                                     // re-estimates (cheap; call every frame)
    void tap(double now);
    void setBpm(double bpm, double now);
    void nudge(double beats);
    // Beat clock fed from outside (DJ software/hardware): bpm and the beat position at time `now`.
    void setExternal(double bpm, double beat, double now);

    double bpm() const;
    double beatAt(double now) const;  // continuous beat count; bars are 4 beats
    float confidence() const;

    void setSource(Source s);
    Source source() const;
    const char* sourceName() const;

    void enableLink(bool on);
    bool linkEnabled() const;
    int linkPeers() const;

private:
    struct Impl;
    Impl* p_;
};
