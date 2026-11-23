#include "media.h"

#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace {
template <class T>
void release(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}
struct MfScope {  // per-thread COM + Media Foundation
    bool ok;
    MfScope() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    }
    ~MfScope() {
        if (ok) MFShutdown();
        CoUninitialize();
    }
};
double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

// ================================================================== Recorder
bool Recorder::start(const std::string& path, int width, int height, int fps, std::string& err, bool offline) {
    stop();
    offline_ = offline;
    w_ = width & ~1;
    h_ = height & ~1;
    fps_ = fps;
    path_ = path;
    frames_ = 0;
    dropped_ = 0;
    error.clear();
    startState_ = 0;
    stopping_ = false;
    active_ = true;
    thread_ = std::thread([this] { run(); });
    while (startState_ == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (startState_ < 0) {
        err = error;
        if (thread_.joinable()) thread_.join();
        active_ = false;
        return false;
    }
    return true;
}

void Recorder::stop() {
    if (!thread_.joinable()) return;
    stopping_ = true;
    cv_.notify_all();
    thread_.join();
    active_ = false;
}

void Recorder::pushFrame(const unsigned char* bgra, const float* stereo, size_t audioFrames) {
    if (!active_ || stopping_) return;
    const size_t size = (size_t)w_ * h_ * 4;
    std::unique_lock<std::mutex> lk(mu_);
    if (offline_) cv_.wait(lk, [&] { return queue_.size() < 6 || stopping_; });
    else if (queue_.size() >= 8) { dropped_++; return; }
    Item it;
    if (!pool_.empty()) { it.video.swap(pool_.back()); pool_.pop_back(); }
    it.video.resize(size);
    lk.unlock();
    memcpy(it.video.data(), bgra, size);
    if (stereo && audioFrames) {
        it.audio.resize(audioFrames * 2);
        for (size_t i = 0; i < audioFrames * 2; i++) it.audio[i] = (short)std::max(-32767.f, std::min(32767.f, stereo[i] * 32767.f));
    }
    lk.lock();
    queue_.push_back(std::move(it));
    cv_.notify_all();
}

void Recorder::run() {
    MfScope mf;
    IMFSinkWriter* writer = nullptr;
    DWORD stream = 0;
    auto fail = [&](const char* what, HRESULT hr) {
        char b[160];
        snprintf(b, sizeof b, "%s (0x%08lX)", what, (unsigned long)hr);
        error = b;
        startState_ = -1;
        release(writer);
    };
    if (!mf.ok) return fail("Media Foundation unavailable", E_FAIL);

    IMFAttributes* attrs = nullptr;
    MFCreateAttributes(&attrs, 2);
    attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attrs->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
    if (offline_) attrs->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);  // offline: never wait for the streams to line up in real time
    HRESULT hr = MFCreateSinkWriterFromURL(plat::widen(path_).c_str(), nullptr, attrs, &writer);
    release(attrs);
    if (FAILED(hr)) return fail("could not create the video file", hr);

    IMFMediaType *out = nullptr, *in = nullptr;
    MFCreateMediaType(&out);
    out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    out->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)std::min<long long>(60000000LL, (long long)w_ * h_ * fps_ / 5));
    out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(out, MF_MT_FRAME_SIZE, w_, h_);
    MFSetAttributeRatio(out, MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    hr = writer->AddStream(out, &stream);
    release(out);
    if (FAILED(hr)) return fail("H.264 encoder rejected the format", hr);

    MFCreateMediaType(&in);
    in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(in, MF_MT_FRAME_SIZE, w_, h_);
    MFSetAttributeRatio(in, MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    in->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)(-(w_ * 4)));  // bottom-up rows, like glReadPixels
    hr = writer->SetInputMediaType(stream, in, nullptr);
    release(in);
    if (FAILED(hr)) return fail("encoder rejected RGB input", hr);
    DWORD aStream = 0;
    if (offline_) {  // AAC 192 kbps from 16-bit 48 kHz stereo PCM
        IMFMediaType *aout = nullptr, *ain = nullptr;
        MFCreateMediaType(&aout);
        aout->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        aout->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
        aout->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        aout->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
        aout->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
        aout->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
        hr = writer->AddStream(aout, &aStream);
        release(aout);
        if (FAILED(hr)) return fail("AAC encoder rejected the format", hr);
        MFCreateMediaType(&ain);
        ain->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        ain->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        ain->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        ain->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
        ain->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
        ain->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
        ain->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192000);
        hr = writer->SetInputMediaType(aStream, ain, nullptr);
        release(ain);
        if (FAILED(hr)) return fail("AAC encoder rejected PCM input", hr);
    }
    long long audioFrames = 0;
    hr = writer->BeginWriting();
    if (FAILED(hr)) return fail("could not start writing", hr);
    startState_ = 1;

    const LONGLONG dur = 10000000LL / fps_;
    const DWORD size = (DWORD)(w_ * h_ * 4);
    while (true) {
        std::vector<unsigned char> frame;
        std::vector<short> audio;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return !queue_.empty() || stopping_; });
            if (queue_.empty() && stopping_) break;
            frame.swap(queue_.front().video);
            audio.swap(queue_.front().audio);
            queue_.pop_front();
            cv_.notify_all();  // a waiting offline pushFrame may continue
        }
        if (offline_ && !audio.empty()) {
            DWORD bytes = (DWORD)(audio.size() * sizeof(short));
            IMFMediaBuffer* ab = nullptr;
            if (SUCCEEDED(MFCreateMemoryBuffer(bytes, &ab))) {
                BYTE* dst = nullptr;
                ab->Lock(&dst, nullptr, nullptr);
                memcpy(dst, audio.data(), bytes);
                ab->Unlock();
                ab->SetCurrentLength(bytes);
                IMFSample* as = nullptr;
                MFCreateSample(&as);
                as->AddBuffer(ab);
                long long n = (long long)audio.size() / 2;
                as->SetSampleTime(audioFrames * 10000000LL / 48000);
                as->SetSampleDuration(n * 10000000LL / 48000);
                if (FAILED(writer->WriteSample(aStream, as))) error = "audio write failed";
                audioFrames += n;
                release(as);
                release(ab);
            }
        }
        IMFMediaBuffer* mb = nullptr;
        if (SUCCEEDED(MFCreateMemoryBuffer(size, &mb))) {
            BYTE* dst = nullptr;
            mb->Lock(&dst, nullptr, nullptr);
            memcpy(dst, frame.data(), size);
            mb->Unlock();
            mb->SetCurrentLength(size);
            IMFSample* s = nullptr;
            MFCreateSample(&s);
            s->AddBuffer(mb);
            s->SetSampleTime(frames_ * dur);
            s->SetSampleDuration(dur);
            if (FAILED(writer->WriteSample(stream, s))) error = "write failed";
            release(s);
            release(mb);
            frames_++;
        }
        std::lock_guard<std::mutex> lk(mu_);
        if (pool_.size() < 8) pool_.push_back(std::move(frame));
    }
    writer->Finalize();
    release(writer);
}

// ================================================================== VideoPlayer
bool VideoPlayer::open(const std::string& path, std::string& err) {
    close();
    auto slash = path.find_last_of("\\/");
    name_ = slash == std::string::npos ? path : path.substr(slash + 1);
    stop_ = false;
    openState_ = 0;
    active_ = true;
    camera_ = -1;
    thread_ = std::thread([this, w = plat::widen(path)] { run(w, -1); });
    while (openState_ == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (openState_ < 0) {
        err = openErr_;
        thread_.join();
        active_ = false;
        return false;
    }
    return true;
}

std::vector<std::string> VideoPlayer::cameras() {
    std::vector<std::string> out;
    MfScope mf;
    IMFAttributes* a = nullptr;
    MFCreateAttributes(&a, 1);
    a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** devs = nullptr;
    UINT32 n = 0;
    if (SUCCEEDED(MFEnumDeviceSources(a, &devs, &n))) {
        for (UINT32 i = 0; i < n; i++) {
            WCHAR* name = nullptr;
            UINT32 len = 0;
            if (SUCCEEDED(devs[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len))) {
                out.push_back(plat::narrow(name));
                CoTaskMemFree(name);
            } else out.push_back("Camera " + std::to_string(i + 1));
            devs[i]->Release();
        }
        CoTaskMemFree(devs);
    }
    release(a);
    return out;
}

bool VideoPlayer::openCamera(int index, std::string& err) {
    close();
    auto names = cameras();
    if (index < 0 || index >= (int)names.size()) { err = "camera not found"; return false; }
    name_ = names[index];
    stop_ = false;
    openState_ = 0;
    active_ = true;
    camera_ = index;
    thread_ = std::thread([this, index] { run(L"", index); });
    while (openState_ == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (openState_ < 0) {
        err = openErr_;
        thread_.join();
        active_ = false;
        camera_ = -1;
        return false;
    }
    return true;
}

void VideoPlayer::close() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    active_ = false;
    std::lock_guard<std::mutex> lk(mu_);
    frame_.clear();
    w_ = h_ = 0;
}

bool VideoPlayer::latest(int& serial, std::vector<unsigned char>& bgra, int& w, int& h, bool& bottomUp) {
    std::lock_guard<std::mutex> lk(mu_);
    if (serial_ == serial || frame_.empty()) return false;
    serial = serial_;
    bgra = frame_;
    w = w_;
    h = h_;
    bottomUp = bottomUp_;
    return true;
}

void VideoPlayer::run(std::wstring path, int camera) {
    MfScope mf;
    IMFSourceReader* reader = nullptr;
    auto fail = [&](const char* what) {
        openErr_ = what;
        openState_ = -1;
        release(reader);
    };
    if (!mf.ok) return fail("Media Foundation unavailable");
    IMFAttributes* attrs = nullptr;
    MFCreateAttributes(&attrs, 1);
    attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    HRESULT hr = E_FAIL;
    if (camera >= 0) {
        IMFAttributes* a = nullptr;
        MFCreateAttributes(&a, 1);
        a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        IMFActivate** devs = nullptr;
        UINT32 n = 0;
        IMFMediaSource* src = nullptr;
        if (SUCCEEDED(MFEnumDeviceSources(a, &devs, &n))) {
            if ((UINT32)camera < n) devs[camera]->ActivateObject(IID_PPV_ARGS(&src));
            for (UINT32 i = 0; i < n; i++) devs[i]->Release();
            CoTaskMemFree(devs);
        }
        release(a);
        if (src) { hr = MFCreateSourceReaderFromMediaSource(src, attrs, &reader); src->Release(); }
        release(attrs);
        if (FAILED(hr)) return fail("could not open the camera (in use by another app?)");
    } else {
        hr = MFCreateSourceReaderFromURL(path.c_str(), attrs, &reader);
        release(attrs);
        if (FAILED(hr)) return fail("could not open the video (mp4, mov, wmv, avi with Windows codecs work)");
    }
    reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    IMFMediaType* t = nullptr;
    MFCreateMediaType(&t);
    t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    t->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, t);
    release(t);
    if (FAILED(hr)) return fail("this video can't be decoded to RGB");
    IMFMediaType* cur = nullptr;
    reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur);
    UINT32 w = 0, h = 0;
    MFGetAttributeSize(cur, MF_MT_FRAME_SIZE, &w, &h);
    INT32 stride = 0;
    if (FAILED(cur->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&stride))) stride = (INT32)w * 4;
    release(cur);
    if (!w || !h) return fail("video has no frame size");
    openState_ = 1;

    double clockStart = nowSec(), pausedAt = 0;
    bool wasPaused = false;
    while (!stop_) {
        if (paused) {
            if (!wasPaused) { pausedAt = nowSec(); wasPaused = true; }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (wasPaused) { clockStart += nowSec() - pausedAt; wasPaused = false; }
        DWORD flags = 0;
        LONGLONG ts = 0;
        IMFSample* s = nullptr;
        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &ts, &s);
        if (FAILED(hr)) break;
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {  // loop
            PROPVARIANT pv;
            PropVariantInit(&pv);
            pv.vt = VT_I8;
            pv.hVal.QuadPart = 0;
            reader->SetCurrentPosition(GUID_NULL, pv);
            PropVariantClear(&pv);
            clockStart = nowSec();
            release(s);
            continue;
        }
        if (!s) continue;
        double due = camera >= 0 ? 0 : clockStart + ts / 1e7;
        while (!stop_ && nowSec() < due - 0.002) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (camera < 0 && nowSec() - due > 1.0) clockStart = nowSec() - ts / 1e7;  // fell far behind: resync
        IMFMediaBuffer* b = nullptr;
        if (SUCCEEDED(s->ConvertToContiguousBuffer(&b))) {
            BYTE* data = nullptr;
            DWORD len = 0;
            if (SUCCEEDED(b->Lock(&data, nullptr, &len))) {
                std::lock_guard<std::mutex> lk(mu_);
                size_t need = (size_t)w * h * 4;
                frame_.resize(need);
                size_t row = (size_t)w * 4, pitch = (size_t)std::abs(stride);
                for (UINT32 y = 0; y < h && (y + 1) * pitch <= len; y++) memcpy(frame_.data() + y * row, data + y * pitch, row);
                w_ = (int)w;
                h_ = (int)h;
                bottomUp_ = stride < 0;
                serial_++;
                b->Unlock();
            }
            release(b);
        }
        release(s);
    }
    release(reader);
}
