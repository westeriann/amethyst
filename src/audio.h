// Audio input for the visualiser: microphone / line-in, system-audio loopback
// ("what you hear", no share dialog) and audio-file playback. Samples land in a
// lock-free ring buffer that the render thread drains every frame.
#pragma once
#include <string>
#include <vector>

struct AudioDevice {
    std::string name;
    bool loopback = false;  // true = capture what this output device plays
    bool isDefault = false;
    int index = -1;         // index within its own list (capture or playback)
};

class AudioEngine {
public:
    enum class Source { None, Input, Loopback, File };

    AudioEngine();
    ~AudioEngine();

    bool init();
    void shutdown();

    std::vector<AudioDevice> devices();  // inputs first, then loopback-able outputs

    bool startInput(int index, std::string& err);     // -1 = system default
    bool startLoopback(int index, std::string& err);  // -1 = default output
    bool startFile(const std::string& path, std::string& err);
    void stop();

    void togglePause();
    bool paused() const;

    // Drain up to maxFrames interleaved stereo float frames. Returns frames read.
    size_t read(float* dst, size_t maxFrames);

    Source source() const { return source_; }
    const std::string& label() const { return label_; }
    const std::string& deviceName() const { return deviceName_; }

    struct Impl;

private:
    Impl* p_;
    Source source_ = Source::None;
    std::string label_ = "No audio";
    std::string deviceName_;
};
