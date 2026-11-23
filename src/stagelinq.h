// Denon DJ StageLinQ client (Engine OS players/mixers: SC LIVE, SC6000, Prime...).
// Reads per-deck BPM, beat position, play state, fader and track title/artist over the network.
// Protocol per the open-source implementations (go-stagelinq, chrisle/StageLinq, PyStageLinQ).
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct DjDeck {
    std::string title, artist;
    double bpm = 0, beat = 0;
    bool playing = false, master = false, hasBeat = false;
    float fader = 1;
    double beatTime = 0;  // local time (seconds) when `beat` was received
};

class StageLinq {
public:
    ~StageLinq() { stop(); }
    void start();
    void stop();
    bool running() const { return running_; }

    struct Snapshot {
        std::string device, status;
        bool connected = false;
        std::vector<DjDeck> decks;  // index 0 = deck 1
    };
    Snapshot snapshot();
    // Deck that is "on air": explicit master, else the loudest playing deck. -1 if none.
    static int onAirDeck(const Snapshot& s);

    // Self-test helper: a minimal fake Engine OS device on localhost.
    static void startFakeDevice(std::atomic<bool>& stop);

private:
    void run();
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::mutex mu_;
    Snapshot snap_;
};
