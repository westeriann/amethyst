#include "player.h"

#include <GL/glew.h>
#include <SDL.h>
#include <projectM-4/projectM.h>

#include <algorithm>

namespace {
struct FailInfo {
    bool failed = false;
    std::string msg;
};
void onFail(const char*, const char* msg, void* user) {
    auto* f = static_cast<FailInfo*>(user);
    f->failed = true;
    f->msg = msg ? msg : "could not load preset";
}
}  // namespace

bool PresetPlayer::start(SDL_Window* hiddenWindow, void* glContext) {
    win_ = hiddenWindow;
    ctx_ = glContext;
    running_ = true;
    thread_ = std::thread([this] { run(); });
    return true;
}

void PresetPlayer::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void PresetPlayer::load(const std::string& path, int token) {
    std::lock_guard<std::mutex> lk(mu_);
    jobs_.clear();  // only the newest request matters
    jobs_.push_back({false, path, token});
    loading_ = true;
    cv_.notify_all();
}

void PresetPlayer::loadData(const std::string& text, int token) {
    std::lock_guard<std::mutex> lk(mu_);
    jobs_.clear();
    jobs_.push_back({true, text, token});
    loading_ = true;
    cv_.notify_all();
}

void PresetPlayer::requestFrame(double frameTime, int w, int h) {
    std::lock_guard<std::mutex> lk(mu_);
    frameReq_ = true;
    frameTime_ = frameTime;
    reqW_ = w;
    reqH_ = h;
    cv_.notify_all();
}

void PresetPlayer::pushAudio(const float* stereo, size_t frames) {
    std::lock_guard<std::mutex> lk(mu_);
    pcm_.insert(pcm_.end(), stereo, stereo + frames * 2);
    const size_t cap = 4096 * 2;
    if (pcm_.size() > cap) pcm_.erase(pcm_.begin(), pcm_.end() - cap);
}

void PresetPlayer::setMesh(int w, int h) {
    std::lock_guard<std::mutex> lk(mu_);
    if (w == meshW_ && h == meshH_) return;
    meshW_ = w;
    meshH_ = h;
    meshDirty_ = true;
}

void PresetPlayer::setTexturePaths(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lk(mu_);
    paths_ = paths;
    pathsDirty_ = true;
}

bool PresetPlayer::takeResult(LoadResult& out) {
    std::lock_guard<std::mutex> lk(mu_);
    if (results_.empty()) return false;
    out = results_.front();
    results_.erase(results_.begin());
    return true;
}

unsigned PresetPlayer::texture() {
    std::lock_guard<std::mutex> lk(mu_);
    if (latest_ < 0) return 0;
    if (fence_[latest_]) glWaitSync((GLsync)fence_[latest_], 0, GL_TIMEOUT_IGNORED);  // GPU-side wait only
    return tex_[latest_];
}

void PresetPlayer::run() {
    SDL_GL_MakeCurrent(win_, (SDL_GLContext)ctx_);
    projectm_handle pm = projectm_create();
    FailInfo fail;
    if (pm) {
        projectm_set_preset_switch_failed_event_callback(pm, onFail, &fail);
        projectm_set_hard_cut_enabled(pm, false);
        projectm_set_preset_duration(pm, 1e9);
        projectm_set_aspect_correction(pm, true);
    }
    GLuint fbo[3] = {0, 0, 0};
    int w = 0, h = 0, framesSinceLoad = 0, pendingToken = 0;
    std::vector<float> pcm;
    const size_t maxSamples = projectm_pcm_get_max_samples();

    while (running_) {
        bool doFrame = false;
        double ft = 0;
        int rw = 0, rh = 0;
        Job job;
        bool haveJob = false, mesh = false, paths = false;
        int mw = 0, mh = 0;
        std::vector<std::string> texPaths;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return !running_ || frameReq_ || !jobs_.empty(); });
            if (!running_) break;
            if (!jobs_.empty()) { job = jobs_.back(); jobs_.clear(); haveJob = true; }
            doFrame = frameReq_;
            frameReq_ = false;
            ft = frameTime_;
            rw = reqW_;
            rh = reqH_;
            pcm.swap(pcm_);
            pcm_.clear();
            if (meshDirty_) { mesh = true; mw = meshW_; mh = meshH_; meshDirty_ = false; }
            if (pathsDirty_) { paths = true; texPaths = paths_; pathsDirty_ = false; }
        }
        if (!pm) continue;
        if (mesh) projectm_set_mesh_size(pm, mw, mh);
        if (paths) {
            std::vector<const char*> c;
            for (auto& p : texPaths) c.push_back(p.c_str());
            if (!c.empty()) projectm_set_texture_search_paths(pm, c.data(), c.size());
        }
        if (haveJob) {
            fail = FailInfo{};
            if (job.data) projectm_load_preset_data(pm, job.payload.c_str(), false);
            else projectm_load_preset_file(pm, job.payload.c_str(), false);
            if (fail.failed) {
                std::lock_guard<std::mutex> lk(mu_);
                results_.push_back({job.token, false, fail.msg});
                if (jobs_.empty()) loading_ = false;
                pendingToken = 0;
            } else {
                pendingToken = job.token;
                framesSinceLoad = 0;
            }
        }
        if (!doFrame || rw <= 0 || rh <= 0) continue;
        if (rw != w || rh != h) {
            w = rw;
            h = rh;
            projectm_set_window_size(pm, w, h);
            for (int i = 0; i < 3; i++) {
                if (!tex_[i]) glGenTextures(1, &tex_[i]);
                glBindTexture(GL_TEXTURE_2D, tex_[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                if (!fbo[i]) glGenFramebuffers(1, &fbo[i]);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_[i], 0);
            }
        }
        size_t frames = pcm.size() / 2;
        const float* audio = pcm.data();
        if (frames > maxSamples) { audio += (frames - maxSamples) * 2; frames = maxSamples; }
        if (frames) projectm_pcm_add_float(pm, audio, (unsigned)frames, PROJECTM_STEREO);
        int slot;
        {
            std::lock_guard<std::mutex> lk(mu_);
            slot = (latest_ + 1) % 3;
        }
        projectm_set_frame_time(pm, ft);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[slot]);
        glViewport(0, 0, w, h);
        projectm_opengl_render_frame_fbo(pm, fbo[slot]);
        GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();  // submit, so other contexts can wait on the fence
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (fence_[slot]) glDeleteSync((GLsync)fence_[slot]);
            fence_[slot] = fence;
            latest_ = slot;
            if (pendingToken && fail.failed) {  // failures can also surface while rendering
                results_.push_back({pendingToken, false, fail.msg});
                pendingToken = 0;
                if (jobs_.empty()) loading_ = false;
            } else if (pendingToken && ++framesSinceLoad >= 2) {  // shaders compiled and warmed up
                results_.push_back({pendingToken, true, ""});
                pendingToken = 0;
                if (jobs_.empty()) loading_ = false;
            }
        }
    }
    for (int i = 0; i < 3; i++) {
        if (fence_[i]) glDeleteSync((GLsync)fence_[i]);
        if (fbo[i]) glDeleteFramebuffers(1, &fbo[i]);
        if (tex_[i]) glDeleteTextures(1, &tex_[i]);
    }
    if (pm) projectm_destroy(pm);
    SDL_GL_MakeCurrent(win_, nullptr);
}
