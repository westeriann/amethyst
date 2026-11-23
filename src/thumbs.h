// Live preset previews. A pool of small projectM instances renders on a worker
// thread with its own (shared) OpenGL context, so shader compiles for previews
// never stall the main output. Every preview is fed the same live audio.
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct SDL_Window;

class ThumbRenderer {
public:
    struct Want {
        int id;
        std::string path;
    };

    static constexpr int kMaxSlots = 48;

    bool start(SDL_Window* hiddenWindow, void* glContext, int width, int height);
    void stop();

    // Presets that should be live right now, most important first.
    void setWanted(std::vector<Want> wanted);
    void setTexturePaths(std::vector<std::string> paths);
    void setFps(int fps) { fps_ = fps; }

    // GL texture showing the preset, or 0 while it is still loading.
    unsigned textureFor(int id);
    void pushAudio(const float* stereo, size_t frames);
    std::vector<std::string> takeFailures();

    // Background analysis of presets (look + energy), used for smart pools and "similar".
    struct Analysis {
        std::string path;
        float hue = 0, sat = 0, bright = 0, motion = 0;  // hue 0..1, others 0..1-ish
        bool failed = false;
    };
    void queueAnalysis(std::vector<std::string> paths);
    std::vector<Analysis> takeAnalysis();
    int analysisPending();
    std::atomic<bool> analysisEnabled{true};

    int width() const { return w_; }
    int height() const { return h_; }

private:
    struct Slot {
        void* pm = nullptr;
        unsigned fbo = 0, tex = 0;
        int id = -1;
        std::string path;
        int frames = 0;
        double lastUsed = 0;
        bool failed = false;
    };

    void run();

    SDL_Window* win_ = nullptr;
    void* ctx_ = nullptr;
    int w_ = 160, h_ = 90;
    std::atomic<int> fps_{30};
    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex mu_;
    std::condition_variable cv_;
    std::vector<Want> wanted_;
    std::vector<float> pcm_;
    std::vector<std::string> texPaths_;
    bool texPathsDirty_ = false;
    std::unordered_map<int, unsigned> ready_;
    std::vector<std::string> failures_;

    std::vector<Slot> slots_;  // worker thread only
    std::vector<std::string> analyzeQueue_;
    std::vector<Analysis> analyzed_;
    bool analyzeOne(const std::string& path, Analysis& out, const std::vector<std::string>& texPaths);
    void* apm_ = nullptr;
    unsigned afbo_ = 0, atex_ = 0;
};
