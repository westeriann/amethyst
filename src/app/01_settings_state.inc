// Part of Amethyst's main.cpp, which includes the parts in order (see the list there).

// ===================================================================== settings
const float kBlendSteps[] = {0, 0.25f, 0.5f, 1, 1.5f, 2, 3, 4, 6, 8, 12};
const int kIntervalSteps[] = {4, 6, 8, 10, 15, 20, 30, 45, 60, 90, 120, 180, 300};
const float kSpeedSteps[] = {0.1f, 0.25f, 0.5f, 0.75f, 1, 1.25f, 1.5f, 2, 3, 4};
struct Quality { const char* label; float scale; int meshW, meshH; };
const Quality kQuality[] = {{"Low", 0.5f, 32, 24}, {"Medium", 0.75f, 48, 36}, {"High", 1.0f, 64, 48}, {"Ultra", 1.0f, 128, 96}};
const char* kQuantizeNames[] = {"Off", "Beat", "Bar", "4 bars"};
const int kQuantizeBeats[] = {0, 1, 4, 16};
const char* kOutputSources[] = {"Final output", "Deck A", "Deck B", "Mix (before FX)"};

struct Settings {
    float blend = 2;
    int interval = 20;
    bool autoOn = false, shuffle = true, autoDecks = false, smartMix = true, fakeoutRecut = true, autoFx = true, autoFxFlash = true;
    float autoFxAmount = 0.7f;   // how hard Auto FX pushes the effects
    float buildTease = 0.35f;    // how far the drop preset fades in during a build-up (0 = off)
    bool smartBlends = true;     // drops smash both presets together with a layer blend
    bool watchdog = true;        // show safety: skip presets that go black, freeze or crawl
    std::string safePreset;      // fallback preset for the watchdog and the panic button
    std::string autoFolder;      // auto-pilot only picks presets inside this folder (empty = anywhere)
    bool vocalEase = true;       // calmer effects and no mid-line switches while someone sings
    std::string userShader;      // custom shader effect picked by hand ("" = none)
    float userShaderAmount = 0.8f;
    bool autoFxShaders = true;   // Auto FX may use custom shaders on drops and phrases
    int quantize = 2;            // auto-pilot launch grid
    bool quantizeManual = false; // also quantize manual launches
    float gain = 1, speed = 1;
    int quality = 2;
    float xfade = 0;
    int mixMode = 0;
    // interface
    bool showMenu = true, showBrowser = true, showHud = true, showCues = true, showToasts = true, showStatus = true;
    bool showMixer = false, showFx = false, showOverlays = false, showOutputs = false, showControl = false, showSetlists = false;
    bool uiHidden = false, livePreviews = true;
    float previewHeight = 64;
    int previewFps = 30;
    // fx
    FxParams fx;
    // overlays
    bool logoOn = false; std::string logoPath; float logoScale = 0.15f, logoOpacity = 0.9f; int logoPos = 3;
    bool textOn = false; std::string text = "AMETHYST"; float textSize = 0.08f, textOpacity = 1, textScroll = 0; int textPos = 2;
    float textR = 1, textG = 1, textB = 1;
    bool videoOn = false; std::string videoPath; float videoOpacity = 0.6f; int videoBlend = 2, videoFit = 0;
    // outputs
    bool spout = false;
    int recFps = 30;
    // tempo
    int tempoSource = 0;
    bool link = false;
    float tapBpm = 120;
    // network
    bool oscOn = false, webOn = false;
    int oscPort = 9000, webPort = 8080;
    // library
    std::vector<std::string> folders, files;
    std::string cues[10];
    std::string lastA, lastB, pool = "all", activeSetlist;
    bool docsFolderAdded = false;
    std::string audioKind = "loopback", audioDevice;
    // transitions, scenes, inputs
    int transition = 1;
    float transBeats = 4, sceneMorphBeats = 2;
    int layerSource = 0, cameraIndex = 0;
    std::string spoutInput;
    bool showScenes = false, showMod = false;
    // lighting
    bool dmxOn = false;
    std::string dmxIp = "255.255.255.255";
    int dmxUniverse = 0, dmxFixtures = 8, dmxStart = 1, dmxMode = 0;
    float dmxLevel = 1;
    // DJ link
    bool djOn = false, djTempo = true, djShowTitle = true;
    int djTrackAction = 0;
    float djTitleSecs = 8;
    // library
    bool hideBroken = false, analysisOn = true;
};

Settings S;
std::string gSettingsPath, gLibraryPath, gImguiIni, gScenesPath, gAnalysisPath;

struct Field { const char* key; char type; void* p; };
std::vector<Field> fields() {
    return {
        {"blend", 'f', &S.blend}, {"interval", 'i', &S.interval}, {"auto", 'b', &S.autoOn}, {"shuffle", 'b', &S.shuffle},
        {"auto_decks", 'b', &S.autoDecks}, {"smart_mix", 'b', &S.smartMix}, {"fakeout_recut", 'b', &S.fakeoutRecut}, {"auto_fx", 'b', &S.autoFx}, {"auto_fx_flash", 'b', &S.autoFxFlash}, {"auto_fx_amount", 'f', &S.autoFxAmount}, {"build_tease", 'f', &S.buildTease}, {"smart_blends", 'b', &S.smartBlends}, {"watchdog", 'b', &S.watchdog}, {"safe_preset", 's', &S.safePreset}, {"auto_folder", 's', &S.autoFolder}, {"vocal_ease", 'b', &S.vocalEase}, {"user_shader", 's', &S.userShader}, {"user_shader_amt", 'f', &S.userShaderAmount}, {"auto_fx_shaders", 'b', &S.autoFxShaders}, {"quantize", 'i', &S.quantize}, {"quantize_manual", 'b', &S.quantizeManual},
        {"gain", 'f', &S.gain}, {"speed", 'f', &S.speed}, {"quality", 'i', &S.quality}, {"xfade", 'f', &S.xfade}, {"mix_mode", 'i', &S.mixMode},
        {"show_menu", 'b', &S.showMenu}, {"show_browser", 'b', &S.showBrowser}, {"show_hud", 'b', &S.showHud}, {"show_cues", 'b', &S.showCues},
        {"show_toasts", 'b', &S.showToasts}, {"show_status", 'b', &S.showStatus}, {"show_mixer", 'b', &S.showMixer}, {"show_fx", 'b', &S.showFx},
        {"show_overlays", 'b', &S.showOverlays}, {"show_outputs", 'b', &S.showOutputs}, {"show_control", 'b', &S.showControl},
        {"show_setlists", 'b', &S.showSetlists}, {"ui_hidden", 'b', &S.uiHidden}, {"live_previews", 'b', &S.livePreviews},
        {"preview_height", 'f', &S.previewHeight}, {"preview_fps", 'i', &S.previewFps},
        {"fx_kaleido", 'b', &S.fx.kaleido}, {"fx_kaleido_n", 'i', &S.fx.kaleidoSegments}, {"fx_mirror", 'b', &S.fx.mirror}, {"fx_mirror_mode", 'i', &S.fx.mirrorMode},
        {"fx_feedback", 'b', &S.fx.feedback}, {"fx_feedback_amt", 'f', &S.fx.feedbackAmount}, {"fx_feedback_zoom", 'f', &S.fx.feedbackZoom},
        {"fx_feedback_rot", 'f', &S.fx.feedbackRotate}, {"fx_rgb", 'b', &S.fx.rgbSplit}, {"fx_rgb_amt", 'f', &S.fx.rgbSplitAmount},
        {"fx_pixel", 'b', &S.fx.pixelate}, {"fx_pixel_size", 'f', &S.fx.pixelSize}, {"fx_invert", 'b', &S.fx.invert},
        {"fx_strobe_rate", 'f', &S.fx.strobeRate}, {"fx_strobe_beat", 'b', &S.fx.strobeOnBeat},
        {"fx_zoom", 'f', &S.fx.zoom}, {"fx_rotate", 'f', &S.fx.rotateSpeed}, {"fx_swirl", 'f', &S.fx.swirl}, {"fx_hue", 'f', &S.fx.hue},
        {"fx_sat", 'f', &S.fx.saturation}, {"fx_contrast", 'f', &S.fx.contrast}, {"fx_bright", 'f', &S.fx.brightness},
        {"logo_on", 'b', &S.logoOn}, {"logo_path", 's', &S.logoPath}, {"logo_scale", 'f', &S.logoScale}, {"logo_opacity", 'f', &S.logoOpacity},
        {"logo_pos", 'i', &S.logoPos}, {"text_on", 'b', &S.textOn}, {"text", 's', &S.text}, {"text_size", 'f', &S.textSize},
        {"text_opacity", 'f', &S.textOpacity}, {"text_scroll", 'f', &S.textScroll}, {"text_pos", 'i', &S.textPos},
        {"text_r", 'f', &S.textR}, {"text_g", 'f', &S.textG}, {"text_b", 'f', &S.textB},
        {"video_on", 'b', &S.videoOn}, {"video_path", 's', &S.videoPath}, {"video_opacity", 'f', &S.videoOpacity},
        {"video_blend", 'i', &S.videoBlend}, {"video_fit", 'i', &S.videoFit},
        {"spout", 'b', &S.spout}, {"rec_fps", 'i', &S.recFps}, {"tempo_source", 'i', &S.tempoSource}, {"link", 'b', &S.link}, {"tap_bpm", 'f', &S.tapBpm},
        {"osc_on", 'b', &S.oscOn}, {"web_on", 'b', &S.webOn}, {"osc_port", 'i', &S.oscPort}, {"web_port", 'i', &S.webPort},
        {"last_a", 's', &S.lastA}, {"last_b", 's', &S.lastB}, {"pool", 's', &S.pool}, {"active_setlist", 's', &S.activeSetlist},
        {"docs_folder_added", 'b', &S.docsFolderAdded}, {"audio_kind", 's', &S.audioKind}, {"audio_device", 's', &S.audioDevice},
        {"transition", 'i', &S.transition}, {"trans_beats", 'f', &S.transBeats}, {"scene_morph_beats", 'f', &S.sceneMorphBeats},
        {"layer_source", 'i', &S.layerSource}, {"camera_index", 'i', &S.cameraIndex}, {"spout_input", 's', &S.spoutInput},
        {"show_scenes", 'b', &S.showScenes}, {"show_mod", 'b', &S.showMod}, {"dmx_on", 'b', &S.dmxOn}, {"dmx_ip", 's', &S.dmxIp},
        {"dmx_universe", 'i', &S.dmxUniverse}, {"dmx_fixtures", 'i', &S.dmxFixtures}, {"dmx_start", 'i', &S.dmxStart}, {"dmx_mode", 'i', &S.dmxMode},
        {"dmx_level", 'f', &S.dmxLevel}, {"dj_on", 'b', &S.djOn}, {"dj_tempo", 'b', &S.djTempo}, {"dj_show_title", 'b', &S.djShowTitle},
        {"dj_track_action", 'i', &S.djTrackAction}, {"dj_title_secs", 'f', &S.djTitleSecs}, {"hide_broken", 'b', &S.hideBroken},
        {"analysis_on", 'b', &S.analysisOn},
    };
}

template <class T, size_t N>
T stepIn(const T (&steps)[N], T value, int dir) {
    size_t best = 0;
    for (size_t i = 0; i < N; i++)
        if (std::fabs((double)steps[i] - value) < std::fabs((double)steps[best] - value)) best = i;
    long j = std::clamp<long>((long)best + dir, 0, (long)N - 1);
    return steps[j];
}

std::vector<std::string> splitBar(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '|') { out.push_back(cur); cur.clear(); } else cur += c;
    }
    out.push_back(cur);
    return out;
}

// ===================================================================== state
struct Preset {
    std::string path, name, lname, source;
    bool broken = false;
};
struct Meta {
    bool fav = false;
    int rating = 0;
    std::vector<std::string> tags;
};
struct Setlist {
    std::string name;
    std::vector<std::string> items;
};

std::vector<Preset> lib;
std::unordered_map<std::string, int> byPath;
std::set<std::string> brokenPaths;
std::map<std::string, Meta> meta;
std::vector<Setlist> setlists;
std::vector<int> view;
std::string query;
char searchBuf[256] = "";
int sourceFilter = -1;
std::vector<std::string> sourceNames;
int sel = 0, upcoming = -1;

// A deck owns two threaded projectM players: the visible one keeps animating while the other loads
// the next preset off the main thread; when it is ready we crossfade (or cut) to it, then swap roles.
struct Deck {
    const char* name;
    PresetPlayer players[2];
    int active = 0;
    bool blending = false;
    double blendStart = 0, blendDur = 0;
    RenderTarget rt;       // only used while blending between the two players
    unsigned outTex = 0;   // what the compositor uses this frame
    int cur = -1;
    // in-flight load
    int pendingIdx = -1, pendingToken = 0, pendingDir = 1, failStreak = 0;
    bool pendingHard = false, pendingRecord = true, pendingAdvance = false, pendingRandom = false, pendingEditor = false;
    bool pendingHold = false;       // keep the loaded preset ready on the spare until releaseHeld()
    bool pendingAuto = false;       // the load was picked by auto-pilot (retries stay in its folder)
    int heldIdx = -1;               // preset waiting on the spare player
    double pendingBlendSecs = -1;   // blend override for this load (-1 = normal)
    float blendManual = -1;         // >= 0: hold the blend at this amount (build-up tease), else time-based
    int blendMode = 0;              // MixCrossfade, or a layer mode (Add, Screen...) for drop smashes and teases
    std::vector<std::string> hist;
    double time = 0, lastSwitch = 0;
    PresetPlayer& visible() { return players[active]; }
    PresetPlayer& spare() { return players[1 - active]; }
};
int loadTokens = 0;
std::vector<SDL_Window*> playerWindows;
std::vector<SDL_GLContext> playerContexts;
Deck decks[2] = {{"A"}, {"B"}};
int focus = 0;
bool locked = false;

struct XfadeAnim { bool active = false; float from = 0, to = 0; double start = 0, dur = 1; } xanim;
struct Launch { bool armed = false; int deck = 0, idx = -1; bool hard = false, random = false, fromAuto = false; double dueBeat = 0; } launch;

SDL_Window* win = nullptr;
SDL_Window* thumbWin = nullptr;
SDL_GLContext ctx = nullptr, thumbCtx = nullptr;
Gfx gfx;
RenderTarget mixRT, fxRT[2], finalRT;
int fxIdx = 0, renderW = 0, renderH = 0;
float fxRotation = 0;

struct Output {
    SDL_Window* win = nullptr;
    int display = -1;
    int source = 0;
    float c[8] = {0, 0, 1, 0, 1, 1, 0, 1};  // TL,TR,BR,BL
    bool grid = false;
};
std::vector<Output> outputs;
int outputSel = 0;

AudioEngine audio;
ThumbRenderer thumbs;
Tempo tempo;
Midi midi;
OscServer osc;
HttpServer web;
Recorder recorder;
VideoPlayer video;
SpoutSender spout;
bool spoutNamed = false;
ActionRegistry actions;

GLuint logoTex = 0, videoTex = 0, pbo[2] = {0, 0};
int logoW = 0, logoH = 0, videoW = 0, videoH = 0, videoSerial = -1, pboIdx = 0;
bool videoBottomUp = false, pboPrimed = false;
std::string loadedLogoPath;
std::vector<unsigned char> videoBuf;
double recAccum = 0;

ImGuiContext *uiCtx = nullptr, *overlayCtx = nullptr;
ImFont *fontBig = nullptr, *fontOverlay = nullptr;

bool running = true, frozen = false, blackoutOn = false;
float blackAmt = 0, flashAmt = 0, level = 0, bassAvg = 0;
double lastBeatHit = 0;
bool helpOpen = false, audioMenuOpen = false, focusSearch = false, defocusSearch = false, scrollToSel = false, searchHasFocus = false;
bool keyboardOpen = false;
// audio bands (filled by pumpAudio, AGC-normalised to ~0..1)
float bandBass = 0, bandMid = 0, bandTreb = 0, bandLevel = 0;
float peakBass = 0.05f, peakMid = 0.05f, peakTreb = 0.05f, peakLevel = 0.05f;
double transitionSeconds();
double beatsToSec(double beats);
void panicSafe();
void startXfade(float to, double dur);
MusicStructure structure;
extern std::string selftestDir;
void testNote(const std::string& s);
// Render an audio file to video, offline and frame-perfect.
struct RenderJob {
    bool active = false, cancel = false, feedNow = false, useAuto = true;
    std::vector<float> pcm;  // 48 kHz stereo
    size_t pos = 0;
    int fps = 60, w = 1920, h = 1080;
    std::string src, out;
    double carry = 0, bpm = 0, wallStart = 0;
    long long frame = 0, total = 0;
    std::vector<float> chunk;
    std::vector<unsigned char> pixels;
    // settings restored afterwards
    bool sAutoOn = false, sSmart = true, sAutoFx = true, sWatchdog = true;
    int sTempoSource = 0;
    // result for the UI / self-test
    std::string lastOut, lastErr;
    double lastSecs = 0, lastWall = 0;
    long long lastFrames = 0;
} rj;
// custom shader effects (ShaderToy-style .glsl files)
struct UserShader {
    std::string name, path, err;
    unsigned prog = 0;
    bool tried = false;
    long long mtime = 0;
};
std::vector<UserShader> userShaders;
int xfadeOnLoad = -1;  // A/B auto-mix: crossfade to this deck once its preset has compiled
double xfadeOnLoadSecs = 0;
std::string captureAction;        // keyboard editor: next key press gets bound to this action
SDL_Window* keyWindow = nullptr;  // window that received the last key press
std::map<std::pair<int, int>, std::string> keymap;
std::vector<std::tuple<int, int, std::string>> savedKeys;
std::map<std::string, SDL_Keycode> heldMomentary;
bool editorOpen = false, editorDirty = false;
VisualPresetEditor visualEditor;
double editorApplyAt = 0;
std::string editorText, editorPath, editorStatus;
int audioMenuSel = 0;
std::vector<AudioDevice> audioDevices;
double lastMouseMove = 0;
std::mt19937 rng{std::random_device{}()};
char tagBuf[128] = "", setlistNameBuf[64] = "";

struct Toast { std::string text; double until; bool err; };
std::vector<Toast> toasts;

// The clock can be driven by the file renderer (one video frame per step); afterwards it carries on
// from where the render left it, so every timestamp stays monotonic.
double clockOverride = -1, clockOffset = 0;
double realNow() { return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency(); }
double now() { return clockOverride >= 0 ? clockOverride : realNow() + clockOffset; }

void toast(bool err, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    toasts.push_back({buf, now() + (err ? 3.5 : 1.8), err});
    if (toasts.size() > 4) toasts.erase(toasts.begin());
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
bool isPresetFile(const fs::path& p) { auto e = lower(p.extension().u8string()); return e == ".milk" || e == ".prjm"; }
bool isAudioFile(const fs::path& p) { auto e = lower(p.extension().u8string()); return e == ".mp3" || e == ".wav" || e == ".flac" || e == ".ogg"; }
bool isVideoFile(const fs::path& p) { auto e = lower(p.extension().u8string()); return e == ".mp4" || e == ".mov" || e == ".wmv" || e == ".avi" || e == ".mkv" || e == ".m4v"; }
bool isImageFile(const fs::path& p) { auto e = lower(p.extension().u8string()); return e == ".png" || e == ".jpg" || e == ".jpeg"; }

// ===================================================================== persistence
void saveSettings() {
    std::ofstream f(fs::u8path(gSettingsPath), std::ios::trunc);
    if (!f) return;
    for (auto& fl : fields()) {
        f << fl.key << "=";
        switch (fl.type) {
            case 'f': f << *(float*)fl.p; break;
            case 'i': f << *(int*)fl.p; break;
            case 'b': f << (*(bool*)fl.p ? 1 : 0); break;
            default: f << *(std::string*)fl.p;
        }
        f << "\n";
    }
    for (auto& d : S.folders) f << "folder=" << d << "\n";
    for (auto& d : S.files) f << "file=" << d << "\n";
    for (int i = 0; i < 10; i++)
        if (!S.cues[i].empty()) f << "cue" << i << "=" << S.cues[i] << "\n";
    for (auto& b : midi.bindings) f << "midi=" << b.action << "|" << b.device << "|" << b.type << "|" << b.channel << "|" << b.number << "\n";
    for (auto& [dev, st] : midi.feedbackStyle) f << "midistyle=" << dev << "|" << st << "\n";
    for (auto& o : outputs) {
        f << "output=" << o.display << "|" << o.source;
        for (float v : o.c) f << "|" << v;
        f << "\n";
    }
    if (!actions.all().empty()) {  // key map is live only after startup
        f << "keymap_saved=1\n";
        for (auto& [k, id] : keymap) f << "key=" << k.first << "|" << k.second << "|" << id << "\n";
    } else {
        for (auto& [k, m, id] : savedKeys)
            if (m >= 0) f << "key=" << k << "|" << m << "|" << id << "\n";
        if (!savedKeys.empty()) f << "keymap_saved=1\n";
    }
}

std::vector<std::vector<std::string>> savedOutputs;

void loadSettings() {
    std::ifstream f(fs::u8path(gSettingsPath));
    if (!f) return;
    auto fl = fields();
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        bool done = false;
        for (auto& x : fl)
            if (k == x.key) {
                switch (x.type) {
                    case 'f': *(float*)x.p = (float)std::atof(v.c_str()); break;
                    case 'i': *(int*)x.p = std::atoi(v.c_str()); break;
                    case 'b': *(bool*)x.p = v == "1"; break;
                    default: *(std::string*)x.p = v;
                }
                done = true;
                break;
            }
        if (done) continue;
        if (k == "folder") S.folders.push_back(v);
        else if (k == "file") S.files.push_back(v);
        else if (k.size() == 4 && k.rfind("cue", 0) == 0 && isdigit((unsigned char)k[3])) S.cues[k[3] - '0'] = v;
        else if (k == "midi") {
            auto p = splitBar(v);
            if (p.size() == 5) midi.bindings.push_back({p[0], p[1], std::atoi(p[2].c_str()), std::atoi(p[3].c_str()), std::atoi(p[4].c_str())});
        } else if (k == "midistyle") {
            auto p = splitBar(v);
            if (p.size() == 2) midi.feedbackStyle[p[0]] = std::atoi(p[1].c_str());
        } else if (k == "output") savedOutputs.push_back(splitBar(v));
        else if (k == "key") {
            auto p = splitBar(v);
            if (p.size() == 3) savedKeys.emplace_back(std::atoi(p[0].c_str()), std::atoi(p[1].c_str()), p[2]);
        } else if (k == "keymap_saved") savedKeys.emplace_back(0, -1, "");  // marker: an (even empty) custom map exists
    }
    S.gain = std::clamp(S.gain, 0.05f, 20.f);
    S.speed = std::clamp(S.speed, 0.1f, 4.f);
    S.quality = std::clamp(S.quality, 0, 3);
    S.interval = std::max(2, S.interval);
    S.quantize = std::clamp(S.quantize, 0, 3);
    S.mixMode = std::clamp(S.mixMode, 0, MixModeCount - 1);
    S.previewHeight = std::clamp(S.previewHeight, 36.f, 160.f);
    S.previewFps = std::clamp(S.previewFps, 5, 60);
}

void saveLibrary() {
    std::ofstream f(fs::u8path(gLibraryPath), std::ios::trunc);
    if (!f) return;
    for (auto& [path, m] : meta) {
        if (!m.fav && !m.rating && m.tags.empty()) continue;
        f << "meta=" << path << "|" << (m.fav ? 1 : 0) << "|" << m.rating << "|";
        for (size_t i = 0; i < m.tags.size(); i++) f << (i ? "," : "") << m.tags[i];
        f << "\n";
    }
    for (auto& s : setlists) {
        f << "setlist=" << s.name << "\n";
        for (auto& it : s.items) f << "item=" << it << "\n";
    }
}

void loadLibrary() {
    std::ifstream f(fs::u8path(gLibraryPath));
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "meta") {
            auto p = splitBar(v);
            if (p.size() < 4) continue;
            Meta m;
            m.fav = p[1] == "1";
            m.rating = std::clamp(std::atoi(p[2].c_str()), 0, 5);
            std::stringstream ss(p[3]);
            for (std::string t; std::getline(ss, t, ',');)
                if (!t.empty()) m.tags.push_back(t);
            meta[p[0]] = m;
        } else if (k == "setlist") setlists.push_back({v, {}});
        else if (k == "item" && !setlists.empty()) setlists.back().items.push_back(v);
    }
}

std::vector<std::string> allTags() {
    std::set<std::string> t;
    for (auto& [p, m] : meta)
        for (auto& x : m.tags) t.insert(x);
    return {t.begin(), t.end()};
}

