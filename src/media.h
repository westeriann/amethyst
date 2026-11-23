// Windows Media Foundation helpers: H.264 MP4 recording and video-file playback (for the
// video overlay layer). Both run on worker threads; the render thread only copies pixels.
#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Recorder {
public:
    ~Recorder() { stop(); }
    bool start(const std::string& path, int width, int height, int fps, std::string& err);
    void stop();
    bool active() const { return active_; }
    // BGRA rows bottom-up (as glReadPixels returns them). Copied; drops if the encoder falls behind.
    void pushFrame(const unsigned char* bgra);
    double seconds() const { return frames_ / (double)fps_; }
    int dropped() const { return dropped_; }
    const std::string& path() const { return path_; }
    int width() const { return w_; }
    int height() const { return h_; }
    int fps() const { return fps_; }
    std::string error;

private:
    void run();
    std::thread thread_;
    std::atomic<bool> active_{false}, stopping_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::vector<unsigned char>> queue_;
    std::vector<std::vector<unsigned char>> pool_;
    std::atomic<long long> frames_{0};
    std::atomic<int> dropped_{0};
    int w_ = 0, h_ = 0, fps_ = 30;
    std::string path_;
    std::atomic<int> startState_{0};  // 0 pending, 1 ok, -1 failed
};

class VideoPlayer {
public:
    ~VideoPlayer() { close(); }
    bool open(const std::string& path, std::string& err);
    bool openCamera(int index, std::string& err);   // live capture device (webcam, capture card)
    static std::vector<std::string> cameras();
    bool isCamera() const { return camera_ >= 0; }
    void close();
    bool active() const { return active_; }
    // Returns true when a newer frame than `serial` exists; copies it (BGRA) and its layout.
    bool latest(int& serial, std::vector<unsigned char>& bgra, int& w, int& h, bool& bottomUp);
    const std::string& name() const { return name_; }
    std::atomic<bool> paused{false};

private:
    void run(std::wstring path, int camera);
    int camera_ = -1;
    std::thread thread_;
    std::atomic<bool> active_{false}, stop_{false};
    std::mutex mu_;
    std::vector<unsigned char> frame_;
    int w_ = 0, h_ = 0, serial_ = 0;
    bool bottomUp_ = false;
    std::string name_;
    std::atomic<int> openState_{0};
    std::string openErr_;
};
