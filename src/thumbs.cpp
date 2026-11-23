#include "thumbs.h"

#include <GL/glew.h>
#include <SDL.h>
#include <projectM-4/projectM.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>

namespace {
void onFail(const char*, const char*, void* user) { *static_cast<bool*>(user) = true; }
double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

bool ThumbRenderer::start(SDL_Window* hiddenWindow, void* glContext, int width, int height) {
    win_ = hiddenWindow;
    ctx_ = glContext;
    w_ = width;
    h_ = height;
    running_ = true;
    thread_ = std::thread([this] { run(); });
    return true;
}

void ThumbRenderer::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void ThumbRenderer::setWanted(std::vector<Want> wanted) {
    std::lock_guard<std::mutex> lk(mu_);
    wanted_ = std::move(wanted);
    cv_.notify_all();
}

void ThumbRenderer::setTexturePaths(std::vector<std::string> paths) {
    std::lock_guard<std::mutex> lk(mu_);
    texPaths_ = std::move(paths);
    texPathsDirty_ = true;
}

unsigned ThumbRenderer::textureFor(int id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = ready_.find(id);
    return it == ready_.end() ? 0 : it->second;
}

void ThumbRenderer::pushAudio(const float* stereo, size_t frames) {
    std::lock_guard<std::mutex> lk(mu_);
    pcm_.insert(pcm_.end(), stereo, stereo + frames * 2);
    const size_t cap = 4096 * 2;  // keep only the most recent audio
    if (pcm_.size() > cap) pcm_.erase(pcm_.begin(), pcm_.end() - cap);
}

void ThumbRenderer::queueAnalysis(std::vector<std::string> paths) {
    std::lock_guard<std::mutex> lk(mu_);
    analyzeQueue_ = std::move(paths);
    std::reverse(analyzeQueue_.begin(), analyzeQueue_.end());  // pop from the back = original order
    cv_.notify_all();
}

std::vector<ThumbRenderer::Analysis> ThumbRenderer::takeAnalysis() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Analysis> out;
    out.swap(analyzed_);
    return out;
}

int ThumbRenderer::analysisPending() {
    std::lock_guard<std::mutex> lk(mu_);
    return (int)analyzeQueue_.size();
}

// Render a preset off-screen for ~1.5 s of synthetic music and measure it.
bool ThumbRenderer::analyzeOne(const std::string& path, Analysis& out, const std::vector<std::string>& texPaths) {
    const int W = 64, H = 36, frames = 72;  // 0.8 s quiet pad, then 120 BPM kicks
    if (!apm_) {
        apm_ = projectm_create();
        if (!apm_) return false;
        auto pm = (projectm_handle)apm_;
        projectm_set_window_size(pm, W, H);
        projectm_set_mesh_size(pm, 24, 18);
        projectm_set_hard_cut_enabled(pm, false);
        projectm_set_preset_duration(pm, 1e9);
        glGenTextures(1, &atex_);
        glBindTexture(GL_TEXTURE_2D, atex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &afbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, afbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atex_, 0);
    }
    auto pm = (projectm_handle)apm_;
    static bool failed = false;
    failed = false;
    projectm_set_preset_switch_failed_event_callback(pm, onFail, &failed);
    std::vector<const char*> c;
    for (auto& p : texPaths) c.push_back(p.c_str());
    if (!c.empty()) projectm_set_texture_search_paths(pm, c.data(), c.size());
    projectm_load_preset_file(pm, path.c_str(), false);
    out.path = path;
    if (failed) { out.failed = true; return true; }
    std::vector<float> pcm(2 * 735);
    std::vector<unsigned char> a((size_t)W * H * 4), b((size_t)W * H * 4), px((size_t)W * H * 4);
    std::vector<float> lum((size_t)W * H, 0.f), D(frames, 0.f);
    static double t0 = 0;
    for (int f = 0; f < frames; f++) {
        for (int s = 0; s < 735; s++) {  // 120 BPM kick + hats + pad
            double t = (f * 735 + s) / 44100.0, ph = std::fmod(t - 0.8, 0.5);
            float kick = t < 0.8 ? 0.f : 1.f;  // first 24 frames: no kick, so the preset settles
            float v = (float)(kick * 0.8 * std::sin(2 * 3.14159 * (55 + 90 * std::exp(-ph * 25)) * ph) * std::exp(-ph * 7) +
                              0.25 * std::sin(2 * 3.14159 * 220 * t) * 0.5 + ((rand() % 2000) / 1000.0 - 1) * 0.08 * std::exp(-std::fmod(t, 0.25) * 30));
            pcm[2 * s] = pcm[2 * s + 1] = v;
        }
        projectm_pcm_add_float(pm, pcm.data(), 735, PROJECTM_STEREO);
        t0 += 1.0 / 30;
        projectm_set_frame_time(pm, t0);
        glBindFramebuffer(GL_FRAMEBUFFER, afbo_);
        glViewport(0, 0, W, H);
        projectm_opengl_render_frame_fbo(pm, afbo_);
        if (failed) { out.failed = true; return true; }
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        double d = 0;
        for (size_t i = 0, j = 0; i < px.size(); i += 4, j++) {
            float l = (px[i] + px[i + 1] + px[i + 2]) / 765.f;
            d += std::fabs(l - lum[j]);
            lum[j] = l;
        }
        D[f] = f > 0 ? (float)(d / (W * H)) : 0.f;
        if (f == frames - 9) a = px;
        if (f == frames - 1) b = px;
    }
    double hx = 0, hy = 0, satSum = 0, valSum = 0, motion = 0;
    for (size_t i = 0; i < b.size(); i += 4) {
        float r = b[i] / 255.f, g = b[i + 1] / 255.f, bl = b[i + 2] / 255.f;
        float mx = std::max(r, std::max(g, bl)), mn = std::min(r, std::min(g, bl)), d = mx - mn;
        float sat = mx > 0 ? d / mx : 0, hue = 0;
        if (d > 1e-4f) {
            if (mx == r) hue = std::fmod((g - bl) / d, 6.f);
            else if (mx == g) hue = (bl - r) / d + 2;
            else hue = (r - g) / d + 4;
            hue /= 6;
            if (hue < 0) hue += 1;
        }
        double w = sat * mx;  // vivid, bright pixels decide the colour
        hx += std::cos(hue * 6.2831853) * w;
        hy += std::sin(hue * 6.2831853) * w;
        satSum += sat;
        valSum += mx;
        float la = (a[i] + a[i + 1] + a[i + 2]) / 765.f, lb = (b[i] + b[i + 1] + b[i + 2]) / 765.f;
        motion += std::fabs(la - lb);
    }
    const double n = (double)W * H;
    double hue = std::atan2(hy, hx) / 6.2831853;
    if (hue < 0) hue += 1;
    out.hue = (float)hue;
    out.sat = (float)(satSum / n);
    out.bright = (float)(valSum / n);
    out.motion = (float)std::min(1.0, motion / n * 6.0);
    // punch: how strongly the picture's movement pulses with the kicks (the 2 Hz part of the frame-to-frame
    // change, whatever the delay), plus how much busier it gets once the kicks start
    auto avg = [&](int a0, int a1) { double s = 0; for (int f = a0; f <= a1; f++) s += D[f]; return s / (a1 - a0 + 1); };
    const int k0 = frames - 45;  // exactly three kick periods of 15 frames
    double dq = avg(12, 23), dk = avg(k0, frames - 1), cs = 0, sn = 0;
    for (int f = k0; f < frames; f++) {
        double ph = 2 * 3.14159265 * (f - 24) / 15.0;
        cs += (D[f] - dk) * std::cos(ph), sn += (D[f] - dk) * std::sin(ph);
    }
    double amp = 2 * std::sqrt(cs * cs + sn * sn) / 45, pulse = amp / (dk + 0.002);
    double react = std::max(0.0, (dk - dq) / (dk + dq + 1e-4));
    double gate = std::min(1.0, dk / 0.003);  // nearly static presets do not punch
    out.punch = (float)std::clamp((0.7 * std::min(1.0, pulse) + 0.3 * std::min(1.0, 2 * react)) * gate, 0.0, 1.0);
    if (const char* lg = getenv("AMETHYST_PUNCH_LOG")) { if (FILE* pf = fopen(lg, "a")) { fprintf(pf, "%.5f %.5f %.3f %.3f %.3f\n", dq, dk, pulse, out.motion, out.punch); fclose(pf); } }
    return true;
}

std::vector<std::string> ThumbRenderer::takeFailures() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    out.swap(failures_);
    return out;
}

void ThumbRenderer::run() {
    SDL_GL_MakeCurrent(win_, (SDL_GLContext)ctx_);
    slots_.resize(kMaxSlots);
    std::vector<Want> wanted;
    std::vector<float> pcm;
    std::vector<std::string> texPaths;
    const size_t maxSamples = projectm_pcm_get_max_samples();

    while (running_) {
        const double frameStart = nowSec();
        bool pathsChanged = false;
        {
            std::unique_lock<std::mutex> lk(mu_);
            if (wanted_.empty() && (analyzeQueue_.empty() || !analysisEnabled)) {
                ready_.clear();
                cv_.wait_for(lk, std::chrono::milliseconds(100));
                continue;
            }
            if (wanted_.empty()) ready_.clear();
            wanted = wanted_;
            pcm.swap(pcm_);
            pcm_.clear();
            if (texPathsDirty_) { texPaths = texPaths_; texPathsDirty_ = false; pathsChanged = true; }
        }
        if (wanted.size() > (size_t)kMaxSlots) wanted.resize(kMaxSlots);

        if (pathsChanged) {
            std::vector<const char*> c;
            for (auto& p : texPaths) c.push_back(p.c_str());
            for (auto& s : slots_)
                if (s.pm) projectm_set_texture_search_paths((projectm_handle)s.pm, c.data(), c.size());
        }

        // Which wanted presets already own a slot?
        std::vector<int> slotOf(wanted.size(), -1);
        std::vector<bool> slotWanted(slots_.size(), false);
        for (size_t i = 0; i < wanted.size(); i++)
            for (size_t s = 0; s < slots_.size(); s++)
                if (slots_[s].id == wanted[i].id && slots_[s].path == wanted[i].path) { slotOf[i] = (int)s; slotWanted[s] = true; break; }

        // Load a couple of new previews per tick (compiles are the expensive part).
        int loads = 2;
        std::vector<std::string> failedNow;
        for (size_t i = 0; i < wanted.size() && loads > 0; i++) {
            if (slotOf[i] >= 0) continue;
            int pick = -1;
            double oldest = 1e300;
            for (size_t s = 0; s < slots_.size(); s++) {
                if (slotWanted[s]) continue;
                if (slots_[s].id < 0) { pick = (int)s; break; }
                if (slots_[s].lastUsed < oldest) { oldest = slots_[s].lastUsed; pick = (int)s; }
            }
            if (pick < 0) break;
            Slot& s = slots_[pick];
            if (!s.pm) {
                s.pm = projectm_create();
                if (!s.pm) break;
                auto pm = (projectm_handle)s.pm;
                projectm_set_window_size(pm, w_, h_);
                projectm_set_mesh_size(pm, 32, 24);
                projectm_set_hard_cut_enabled(pm, false);
                projectm_set_preset_duration(pm, 1e9);
                projectm_set_preset_switch_failed_event_callback(pm, onFail, &s.failed);
                std::vector<const char*> c;
                for (auto& p : texPaths) c.push_back(p.c_str());
                if (!c.empty()) projectm_set_texture_search_paths(pm, c.data(), c.size());
                glGenTextures(1, &s.tex);
                glBindTexture(GL_TEXTURE_2D, s.tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glGenFramebuffers(1, &s.fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.tex, 0);
            }
            s.failed = false;
            projectm_load_preset_file((projectm_handle)s.pm, wanted[i].path.c_str(), false);
            loads--;
            if (s.failed) {
                failedNow.push_back(wanted[i].path);
                s.id = -1;
                s.path.clear();
                continue;
            }
            s.id = wanted[i].id;
            s.path = wanted[i].path;
            s.frames = 0;
            slotOf[i] = pick;
            slotWanted[pick] = true;
        }

        // Render every wanted preview with the latest audio.
        const double t = nowSec();
        size_t frames = pcm.size() / 2;
        const float* audio = pcm.data();
        if (frames > maxSamples) { audio += (frames - maxSamples) * 2; frames = maxSamples; }
        std::unordered_map<int, unsigned> ready;
        for (size_t i = 0; i < wanted.size(); i++) {
            if (slotOf[i] < 0) continue;
            Slot& s = slots_[slotOf[i]];
            auto pm = (projectm_handle)s.pm;
            if (frames) projectm_pcm_add_float(pm, audio, (unsigned)frames, PROJECTM_STEREO);
            glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
            glViewport(0, 0, w_, h_);
            projectm_opengl_render_frame_fbo(pm, s.fbo);
            if (s.failed) { failedNow.push_back(s.path); s.id = -1; s.path.clear(); continue; }
            s.frames++;
            s.lastUsed = t;
            if (s.frames >= 2) ready[s.id] = s.tex;
        }
        // one analysis job per tick, in the time left over
        if (analysisEnabled) {
            std::string job;
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (!analyzeQueue_.empty()) { job = analyzeQueue_.back(); analyzeQueue_.pop_back(); }
            }
            if (!job.empty()) {
                Analysis an;
                if (analyzeOne(job, an, texPaths)) {
                    std::lock_guard<std::mutex> lk(mu_);
                    analyzed_.push_back(an);
                }
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFinish();  // make the frames visible to the UI context

        {
            std::lock_guard<std::mutex> lk(mu_);
            ready_.swap(ready);
            failures_.insert(failures_.end(), failedNow.begin(), failedNow.end());
        }

        const double budget = 1.0 / std::max(1, fps_.load());
        const double left = budget - (nowSec() - frameStart);
        if (left > 0) {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait_for(lk, std::chrono::duration<double>(left));
        }
    }

    if (apm_) projectm_destroy((projectm_handle)apm_);
    if (afbo_) glDeleteFramebuffers(1, &afbo_);
    if (atex_) glDeleteTextures(1, &atex_);
    for (auto& s : slots_) {
        if (s.pm) projectm_destroy((projectm_handle)s.pm);
        if (s.fbo) glDeleteFramebuffers(1, &s.fbo);
        if (s.tex) glDeleteTextures(1, &s.tex);
    }
    slots_.clear();
    SDL_GL_MakeCurrent(win_, nullptr);
}
