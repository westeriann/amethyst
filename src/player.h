// One projectM instance running on its own thread with its own (shared) OpenGL context.
// Loading a preset (parse + HLSL->GLSL + shader compile, up to ~1 s for heavy presets) blocks only
// this thread; the main thread keeps compositing the last finished frame. Frames go into a ring of
// shared textures guarded by GL fence objects.
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct SDL_Window;

class PresetPlayer {
public:
    struct LoadResult {
        int token = 0;
        bool ok = false;
        std::string error;
    };

    bool start(SDL_Window* hiddenWindow, void* glContext);
    void stop();

    // ---- main thread API (all non-blocking)
    void load(const std::string& path, int token);       // .milk / .prjm file
    void loadData(const std::string& text, int token);   // preset text (editor)
    void requestFrame(double frameTime, int w, int h);   // render one frame soon
    void pushAudio(const float* stereo, size_t frames);
    void setMesh(int w, int h);
    void setTexturePaths(const std::vector<std::string>& paths);
    bool takeResult(LoadResult& out);                     // a load finished (ok = rendered its first frames)
    bool loading() const { return loading_; }
    // Latest finished frame, made safe to sample on the calling (main) context. 0 = nothing yet.
    unsigned texture();

private:
    void run();
    struct Job { bool data = false; std::string payload; int token = 0; };

    SDL_Window* win_ = nullptr;
    void* ctx_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false}, loading_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    // requests
    bool frameReq_ = false;
    double frameTime_ = 0;
    int reqW_ = 0, reqH_ = 0;
    std::vector<Job> jobs_;
    std::vector<float> pcm_;
    int meshW_ = 64, meshH_ = 48;
    bool meshDirty_ = true, pathsDirty_ = false;
    std::vector<std::string> paths_;
    std::vector<LoadResult> results_;
    // published frames (ring of 3)
    unsigned tex_[3] = {0, 0, 0};
    void* fence_[3] = {nullptr, nullptr, nullptr};
    int latest_ = -1;
};
