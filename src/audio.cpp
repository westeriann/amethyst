#include "audio.h"

#include "platform.h"

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#include <atomic>
#include <cstring>

static constexpr ma_uint32 kRate = 48000;
static constexpr ma_uint32 kRingFrames = kRate;  // 1 s of headroom

struct AudioEngine::Impl {
    ma_context ctx{};
    bool ctxOk = false;
    ma_device dev{};
    bool devOk = false;
    ma_decoder dec{};
    bool decOk = false;
    ma_pcm_rb rb{};
    bool rbOk = false;
    std::atomic<bool> paused{false};
    std::vector<ma_device_info> capture, playback;

    void push(const float* src, ma_uint32 frames) {
        while (frames) {
            ma_uint32 n = frames;
            void* buf = nullptr;
            if (ma_pcm_rb_acquire_write(&rb, &n, &buf) != MA_SUCCESS || n == 0) return;  // full: drop
            memcpy(buf, src, n * 2 * sizeof(float));
            ma_pcm_rb_commit_write(&rb, n);
            src += n * 2;
            frames -= n;
        }
    }
};

static void onData(ma_device* d, void* out, const void* in, ma_uint32 frames) {
    auto* I = static_cast<AudioEngine::Impl*>(d->pUserData);
    if (d->type == ma_device_type_playback) {
        float* o = static_cast<float*>(out);
        if (I->paused || !I->decOk) {
            memset(o, 0, frames * 2 * sizeof(float));
            return;
        }
        ma_uint64 got = 0;
        ma_decoder_read_pcm_frames(&I->dec, o, frames, &got);
        if (got < frames) {  // loop the track
            ma_decoder_seek_to_pcm_frame(&I->dec, 0);
            ma_uint64 more = 0;
            ma_decoder_read_pcm_frames(&I->dec, o + got * 2, frames - got, &more);
            got += more;
            if (got < frames) memset(o + got * 2, 0, (frames - got) * 2 * sizeof(float));
        }
        I->push(o, frames);
    } else if (in) {
        I->push(static_cast<const float*>(in), frames);
    }
}

AudioEngine::AudioEngine() : p_(new Impl) {}
AudioEngine::~AudioEngine() {
    shutdown();
    delete p_;
}

bool AudioEngine::init() {
    if (p_->ctxOk) return true;
    if (ma_context_init(nullptr, 0, nullptr, &p_->ctx) != MA_SUCCESS) return false;
    p_->ctxOk = true;
    if (ma_pcm_rb_init(ma_format_f32, 2, kRingFrames, nullptr, nullptr, &p_->rb) == MA_SUCCESS) p_->rbOk = true;
    return p_->rbOk;
}

void AudioEngine::shutdown() {
    stop();
    if (p_->rbOk) { ma_pcm_rb_uninit(&p_->rb); p_->rbOk = false; }
    if (p_->ctxOk) { ma_context_uninit(&p_->ctx); p_->ctxOk = false; }
}

std::vector<AudioDevice> AudioEngine::devices() {
    std::vector<AudioDevice> out;
    if (!p_->ctxOk) return out;
    ma_device_info *play = nullptr, *cap = nullptr;
    ma_uint32 nPlay = 0, nCap = 0;
    if (ma_context_get_devices(&p_->ctx, &play, &nPlay, &cap, &nCap) != MA_SUCCESS) return out;
    p_->capture.assign(cap, cap + nCap);
    p_->playback.assign(play, play + nPlay);
    for (ma_uint32 i = 0; i < nCap; i++) out.push_back({cap[i].name, false, cap[i].isDefault != 0, (int)i});
    for (ma_uint32 i = 0; i < nPlay; i++) out.push_back({play[i].name, true, play[i].isDefault != 0, (int)i});
    return out;
}

static bool startDevice(AudioEngine::Impl* I, ma_device_config& cfg, std::string& err) {
    cfg.dataCallback = onData;
    cfg.pUserData = I;
    cfg.sampleRate = kRate;
    cfg.performanceProfile = ma_performance_profile_low_latency;
    if (ma_device_init(&I->ctx, &cfg, &I->dev) != MA_SUCCESS) {
        err = "could not open the audio device";
        return false;
    }
    I->devOk = true;
    if (ma_device_start(&I->dev) != MA_SUCCESS) {
        ma_device_uninit(&I->dev);
        I->devOk = false;
        err = "could not start the audio device";
        return false;
    }
    return true;
}

bool AudioEngine::startInput(int index, std::string& err) {
    stop();
    devices();
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 2;
    if (index >= 0 && index < (int)p_->capture.size()) cfg.capture.pDeviceID = &p_->capture[index].id;
    if (!startDevice(p_, cfg, err)) return false;
    deviceName_ = (index >= 0 && index < (int)p_->capture.size()) ? p_->capture[index].name : "";
    source_ = Source::Input;
    label_ = "Input: " + (deviceName_.empty() ? std::string("default") : deviceName_);
    return true;
}

bool AudioEngine::startLoopback(int index, std::string& err) {
    stop();
    devices();
    ma_device_config cfg = ma_device_config_init(ma_device_type_loopback);
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 2;
    if (index >= 0 && index < (int)p_->playback.size()) cfg.capture.pDeviceID = &p_->playback[index].id;
    if (!startDevice(p_, cfg, err)) return false;
    deviceName_ = (index >= 0 && index < (int)p_->playback.size()) ? p_->playback[index].name : "";
    source_ = Source::Loopback;
    label_ = "System audio: " + (deviceName_.empty() ? std::string("default output") : deviceName_);
    return true;
}

bool AudioEngine::startFile(const std::string& path, std::string& err) {
    stop();
    ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 2, kRate);
    if (ma_decoder_init_file_w(plat::widen(path).c_str(), &dc, &p_->dec) != MA_SUCCESS) {
        err = "unsupported or unreadable audio file (mp3, wav and flac work)";
        return false;
    }
    p_->decOk = true;
    p_->paused = false;
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    if (!startDevice(p_, cfg, err)) {
        ma_decoder_uninit(&p_->dec);
        p_->decOk = false;
        return false;
    }
    auto slash = path.find_last_of("\\/");
    deviceName_ = path;
    source_ = Source::File;
    label_ = "File: " + (slash == std::string::npos ? path : path.substr(slash + 1));
    return true;
}

void AudioEngine::stop() {
    if (p_->devOk) { ma_device_uninit(&p_->dev); p_->devOk = false; }
    if (p_->decOk) { ma_decoder_uninit(&p_->dec); p_->decOk = false; }
    if (p_->rbOk) ma_pcm_rb_reset(&p_->rb);
    source_ = Source::None;
    label_ = "No audio";
    deviceName_.clear();
}

void AudioEngine::togglePause() { p_->paused = !p_->paused; }
bool AudioEngine::paused() const { return p_->paused; }

size_t AudioEngine::read(float* dst, size_t maxFrames) {
    if (!p_->rbOk) return 0;
    size_t total = 0;
    while (total < maxFrames) {
        ma_uint32 n = (ma_uint32)(maxFrames - total);
        void* buf = nullptr;
        if (ma_pcm_rb_acquire_read(&p_->rb, &n, &buf) != MA_SUCCESS || n == 0) break;
        memcpy(dst + total * 2, buf, n * 2 * sizeof(float));
        ma_pcm_rb_commit_read(&p_->rb, n);
        total += n;
    }
    return total;
}
