// Amethyst (native) — keyboard-, MIDI- and remote-driven MilkDrop VJ software built on libprojectM.
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <projectM-4/projectM.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

#include "SpoutReceiver.h"
#include "SpoutSender.h"

#include "actions.h"
#include "image.h"
#include "audio.h"
#include "gfx.h"
#include "media.h"
#include "midi.h"
#include "net.h"
#include "platform.h"
#include "player.h"
#include "preset_editor.h"
#include "structure.h"
#include "remote_page.h"
#include "stagelinq.h"
#include "tempo.h"
#include "thumbs.h"

#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <tuple>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

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
    bool autoOn = false, shuffle = true, autoDecks = false, smartMix = true, fakeoutRecut = true;
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
        {"auto_decks", 'b', &S.autoDecks}, {"smart_mix", 'b', &S.smartMix}, {"fakeout_recut", 'b', &S.fakeoutRecut}, {"quantize", 'i', &S.quantize}, {"quantize_manual", 'b', &S.quantizeManual},
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
    int heldIdx = -1;               // preset waiting on the spare player
    double pendingBlendSecs = -1;   // blend override for this load (-1 = normal)
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
void startXfade(float to, double dur);
MusicStructure structure;
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

double now() { return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency(); }

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

// ===================================================================== library
std::vector<std::string> texturePaths() {
    std::vector<std::string> out;
    auto add = [&](const fs::path& p) {
        std::error_code ec;
        if (fs::is_directory(p, ec)) out.push_back(p.u8string());
    };
    add(fs::u8path(plat::exeDir()) / "textures");
    add(fs::u8path(plat::documentsDir()) / "Amethyst" / "Textures");
    for (auto& d : S.folders) {
        add(fs::u8path(d));
        add(fs::u8path(d) / "textures");
        add(fs::u8path(d) / "Textures");
    }
    return out;
}

void applyTexturePaths() {
    auto paths = texturePaths();
    std::vector<const char*> c;
    for (auto& p : paths) c.push_back(p.c_str());
    for (auto& d : decks)
        for (auto& p : d.players) p.setTexturePaths(paths);
    thumbs.setTexturePaths(paths);
}

const Setlist* poolSetlist() {
    if (S.pool.rfind("setlist:", 0) != 0) return nullptr;
    std::string n = S.pool.substr(8);
    for (auto& s : setlists)
        if (s.name == n) return &s;
    return nullptr;
}

std::string poolLabel() {
    if (S.pool == "all") return "All presets";
    if (S.pool == "fav") return "Favorites";
    if (S.pool.rfind("rating:", 0) == 0) return std::string(S.pool.substr(7)) + "+ stars";
    if (S.pool.rfind("tag:", 0) == 0) return "Tag: " + S.pool.substr(4);
    if (S.pool.rfind("setlist:", 0) == 0) return "Setlist: " + S.pool.substr(8);
    if (S.pool.rfind("color:", 0) == 0) return "Colour: " + S.pool.substr(6);
    if (S.pool == "energy:calm") return "Calm";
    if (S.pool == "energy:intense") return "Intense";
    if (S.pool == "light:dark") return "Dark";
    if (S.pool == "light:bright") return "Bright";
    if (S.pool.rfind("similar:", 0) == 0) return "Similar to " + fs::u8path(S.pool.substr(8)).stem().u8string();
    return S.pool;
}

bool smartPoolMatch(int i);
void sortSimilar(std::vector<int>& v);
void queueAnalysis();
bool inPool(int i) {
    const Preset& p = lib[i];
    if (S.hideBroken && p.broken) return false;
    if (S.pool.rfind("color:", 0) == 0 || S.pool.rfind("energy:", 0) == 0 || S.pool.rfind("light:", 0) == 0) {
        if (sourceFilter >= 0 && (sourceFilter >= (int)sourceNames.size() || p.source != sourceNames[sourceFilter])) return false;
        return smartPoolMatch(i);
    }
    if (sourceFilter >= 0 && (sourceFilter >= (int)sourceNames.size() || p.source != sourceNames[sourceFilter])) return false;
    if (S.pool == "all" || S.pool.rfind("setlist:", 0) == 0) return true;
    auto it = meta.find(p.path);
    if (S.pool == "fav") return it != meta.end() && it->second.fav;
    if (S.pool.rfind("rating:", 0) == 0) return it != meta.end() && it->second.rating >= std::atoi(S.pool.c_str() + 7);
    if (S.pool.rfind("tag:", 0) == 0) {
        if (it == meta.end()) return false;
        auto& t = it->second.tags;
        return std::find(t.begin(), t.end(), S.pool.substr(4)) != t.end();
    }
    return true;
}

void rebuildView() {
    std::vector<std::string> terms;
    std::istringstream ss(lower(query));
    for (std::string t; ss >> t;) terms.push_back(t);
    auto match = [&](int i) {
        if (!inPool(i)) return false;
        for (auto& t : terms)
            if (lib[i].lname.find(t) == std::string::npos) return false;
        return true;
    };
    view.clear();
    if (const Setlist* sl = poolSetlist()) {  // setlists keep their own order
        for (auto& path : sl->items) {
            auto it = byPath.find(path);
            if (it != byPath.end() && match(it->second)) view.push_back(it->second);
        }
    } else {
        for (int i = 0; i < (int)lib.size(); i++)
            if (match(i)) view.push_back(i);
        if (S.pool.rfind("similar:", 0) == 0) sortSimilar(view);
    }
    upcoming = -1;  // picked from the old filters: auto-pilot must stay inside the new ones
    int cur = decks[focus].cur;
    auto it = std::find(view.begin(), view.end(), cur);
    sel = it == view.end() ? 0 : (int)(it - view.begin());
    scrollToSel = true;
}

void setPool(const std::string& p) {
    S.pool = p;
    rebuildView();
    toast(false, "Pool: %s (%d)", poolLabel().c_str(), (int)view.size());
}

void rescan() {
    std::string curPath[2];
    for (int d = 0; d < 2; d++) curPath[d] = decks[d].cur >= 0 ? lib[decks[d].cur].path : (d ? S.lastB : S.lastA);
    lib.clear();
    byPath.clear();
    sourceNames.clear();
    std::set<std::string> sources;
    auto addOne = [&](const fs::path& p, const std::string& source) {
        auto u = p.u8string();
        if (byPath.count(u)) return;
        Preset pr;
        pr.path = u;
        pr.name = p.stem().u8string();
        pr.lname = lower(pr.name);
        pr.source = source;
        pr.broken = brokenPaths.count(u) > 0;
        byPath[u] = (int)lib.size();
        lib.push_back(std::move(pr));
        sources.insert(source);
    };
    for (auto& d : S.folders) {
        fs::path root = fs::u8path(d);
        std::string label = root.filename().u8string();
        if (label.empty()) label = d;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code e2;
            if (!it->is_regular_file(e2) || !isPresetFile(it->path())) continue;
            // label = up to two sub-folder levels (e.g. "Cream of the Crop / Fractal"), else the folder name
            fs::path relDir = it->path().parent_path().lexically_relative(root);
            std::string src;
            int depth = 0;
            for (auto& part : relDir) {
                std::string s = part.u8string();
                if (s.empty() || s == ".") continue;
                src += (src.empty() ? "" : " / ") + s;
                if (++depth == 2) break;
            }
            addOne(it->path(), src.empty() ? label : src);
        }
    }
    for (auto& f : S.files) {
        std::error_code ec;
        if (fs::exists(fs::u8path(f), ec)) addOne(fs::u8path(f), "Single files");
    }
    sourceNames.assign(sources.begin(), sources.end());
    std::sort(lib.begin(), lib.end(), [](const Preset& a, const Preset& b) { return a.lname < b.lname; });
    byPath.clear();
    for (int i = 0; i < (int)lib.size(); i++) byPath[lib[i].path] = i;
    for (int d = 0; d < 2; d++) {
        auto it = byPath.find(curPath[d]);
        decks[d].cur = it == byPath.end() ? -1 : it->second;
    }
    if (sourceFilter >= (int)sourceNames.size()) sourceFilter = -1;
    upcoming = -1;
    rebuildView();
    applyTexturePaths();
    queueAnalysis();
}

void addFolder(const std::string& d) {
    if (d.empty()) return;
    if (std::find(S.folders.begin(), S.folders.end(), d) == S.folders.end()) S.folders.push_back(d);
    size_t before = lib.size();
    rescan();
    saveSettings();
    toast(false, "Added folder — %d presets found", (int)(lib.size() - before));
}

void markBroken(int idx, const std::string& why) {
    if (idx < 0 || idx >= (int)lib.size()) return;
    lib[idx].broken = true;
    brokenPaths.insert(lib[idx].path);
    fprintf(stderr, "[Amethyst] preset failed: %s (%s)\n", lib[idx].path.c_str(), why.c_str());
}

Meta& metaOf(int idx) { return meta[lib[idx].path]; }

// ===================================================================== decks & navigation
int stepFrom(int base, int dir) {
    if (view.empty()) return -1;
    auto it = std::find(view.begin(), view.end(), base);
    int i = it == view.end() ? (dir > 0 ? -1 : (int)view.size()) : (int)(it - view.begin());
    for (size_t n = 0; n < view.size(); n++) {
        i = (i + dir + (int)view.size()) % (int)view.size();
        if (!lib[view[i]].broken) return view[i];
    }
    return -1;
}

int pickRandom(int exclude) {
    if (view.empty()) return -1;
    std::uniform_int_distribution<size_t> d(0, view.size() - 1);
    for (int n = 0; n < 24; n++) {
        int i = view[d(rng)];
        if (!lib[i].broken && i != exclude && i != decks[0].cur && i != decks[1].cur) return i;
    }
    return stepFrom(exclude, 1);
}

// Queue a preset on the deck's spare player. Never blocks: the switch happens in commitLoads()
// once the preset has compiled and rendered its first frames.
bool loadPreset(int d, int idx, bool hard, bool record = true) {
    Deck& dk = decks[d];
    if (idx < 0 || idx >= (int)lib.size()) return false;
    if (dk.blending) { dk.blending = false; }  // the spare is about to be reused: finish the running blend
    dk.pendingIdx = idx;
    dk.pendingToken = ++loadTokens;
    dk.pendingHard = hard;
    dk.pendingRecord = record;
    dk.pendingAdvance = false;
    dk.pendingEditor = false;
    dk.pendingHold = false;
    dk.heldIdx = -1;
    dk.pendingBlendSecs = -1;
    dk.spare().load(lib[idx].path, dk.pendingToken);
    if (d == focus) {
        auto it = std::find(view.begin(), view.end(), idx);
        if (it != view.end()) { sel = (int)(it - view.begin()); scrollToSel = true; }
    }
    return true;
}

int navBase(int d) { return decks[d].pendingIdx >= 0 ? decks[d].pendingIdx : decks[d].cur; }

void go(int d, int dir, bool hard, bool random = false) {
    if (view.empty()) { toast(true, query.empty() ? "No presets in this pool — add a folder (Ctrl+Shift+O)" : "No presets match the search"); return; }
    int target;
    if (random) {
        bool upOk = upcoming >= 0 && upcoming != decks[d].cur && !lib[upcoming].broken && std::find(view.begin(), view.end(), upcoming) != view.end();
        target = upOk ? upcoming : pickRandom(navBase(d));
        upcoming = -1;
    } else {
        target = stepFrom(navBase(d), dir);
    }
    if (target < 0) return;
    loadPreset(d, target, hard);
    decks[d].pendingAdvance = true;  // if it fails to load, try the next one
    decks[d].pendingDir = dir;
    decks[d].pendingRandom = random;
}

// Called every frame: finish loads that the player threads completed.
void commitLoads() {
    for (int d = 0; d < 2; d++) {
        Deck& dk = decks[d];
        PresetPlayer::LoadResult r;
        while (dk.spare().takeResult(r)) {
            if (r.token != dk.pendingToken) continue;  // superseded by a newer request
            int idx = dk.pendingIdx;
            dk.pendingIdx = -1;
            dk.pendingToken = 0;
            if (dk.pendingEditor) {
                editorStatus = r.ok ? "Applied to deck " + std::string(dk.name) : "Error: " + r.error;
                if (r.ok) { dk.active = 1 - dk.active; dk.blending = false; }
                continue;
            }
            if (!r.ok) {
                if (idx >= 0) {
                    markBroken(idx, r.error);
                    toast(true, "Skipped \"%s\" — %s", lib[idx].name.c_str(), r.error.c_str());
                }
                if (dk.pendingAdvance && ++dk.failStreak < 12) go(d, dk.pendingDir, dk.pendingHard, dk.pendingRandom);
                continue;
            }
            dk.failStreak = 0;
            if (dk.pendingHold) {
                dk.pendingHold = false;
                dk.heldIdx = idx;
                continue;
            }
            if (dk.pendingRecord && dk.cur >= 0 && dk.cur != idx) {
                dk.hist.push_back(lib[dk.cur].path);
                if (dk.hist.size() > 300) dk.hist.erase(dk.hist.begin());
            }
            dk.cur = idx;
            dk.lastSwitch = now();
            if (idx >= 0) (d ? S.lastB : S.lastA) = lib[idx].path;
            dk.active = 1 - dk.active;  // the freshly loaded player becomes visible
            double bd = dk.pendingBlendSecs >= 0 ? dk.pendingBlendSecs : S.blend;
            dk.blending = !dk.pendingHard && bd > 0.01;
            dk.blendStart = now();
            dk.blendDur = bd;
            if (xfadeOnLoad == d) {
                startXfade(d ? 1.f : 0.f, xfadeOnLoadSecs);
                xfadeOnLoad = -1;
            }
        }
    }
}

bool anyLoading() { return decks[0].pendingIdx >= 0 || decks[1].pendingIdx >= 0; }

void back(int d, bool hard) {
    Deck& dk = decks[d];
    while (!dk.hist.empty()) {
        auto it = byPath.find(dk.hist.back());
        dk.hist.pop_back();
        if (it != byPath.end() && !lib[it->second].broken && it->second != dk.cur) {
            if (loadPreset(d, it->second, hard, false)) return;
        }
    }
    go(d, -1, hard);
}

double beatNow() { return tempo.beatAt(now()); }

// Launch now, or at the next beat/bar boundary when quantizing.
void requestLaunch(int d, int idx, int dir, bool hard, bool random, bool fromAuto) {
    int q = S.quantize;
    bool quantized = q > 0 && (fromAuto || S.quantizeManual) && !hard;
    if (!quantized) {
        if (idx >= 0) loadPreset(d, idx, hard);
        else if (random) go(d, 1, hard, true);
        else if (dir < 0) back(d, hard);
        else go(d, 1, hard);
        return;
    }
    double unit = kQuantizeBeats[q];
    double b = beatNow();
    launch = {true, d, idx, hard, random, fromAuto, std::floor(b / unit + 1e-6) * unit + unit};
    if (idx < 0 && !random) launch.idx = dir < 0 ? -2 : stepFrom(decks[d].cur, 1);
}

void startXfade(float to, double dur) {
    xanim = {true, S.xfade, to, now(), std::max(0.05, dur)};
}

void autoAdvance() {
    if (S.autoDecks) {
        int off = S.xfade < 0.5f ? 1 : 0;
        int target = S.shuffle ? pickRandom(decks[off].cur) : stepFrom(decks[1 - off].cur, 1);
        if (target >= 0 && loadPreset(off, target, true)) {
            xfadeOnLoad = off;
            xfadeOnLoadSecs = transitionSeconds();
        }
    } else {
        int on = S.xfade < 0.5f ? 0 : 1;
        requestLaunch(on, -1, 1, false, S.shuffle, false);
    }
}

void fireCue(int slot) {
    int label = (slot + 1) % 10;
    if (S.cues[slot].empty()) { toast(false, "Cue %d is empty — Shift+%d stores the current preset", label, label); return; }
    auto it = byPath.find(S.cues[slot]);
    if (it == byPath.end()) { toast(true, "Cue %d: preset is no longer in the library", label); return; }
    requestLaunch(focus, it->second, 1, false, false, false);
}
void storeCue(int slot) {
    int cur = decks[focus].cur;
    if (cur < 0) return;
    S.cues[slot] = lib[cur].path;
    saveSettings();
    toast(false, "Stored in cue %d: %s", (slot + 1) % 10, lib[cur].name.c_str());
}
void clearCue(int slot) {
    S.cues[slot].clear();
    saveSettings();
    toast(false, "Cleared cue %d", (slot + 1) % 10);
}

void toggleFavorite(int idx) {
    if (idx < 0) return;
    Meta& m = metaOf(idx);
    m.fav = !m.fav;
    saveLibrary();
    toast(false, m.fav ? "Favorited: %s" : "Unfavorited: %s", lib[idx].name.c_str());
    if (S.pool == "fav") rebuildView();
}
void setRating(int idx, int r) {
    if (idx < 0) return;
    metaOf(idx).rating = std::clamp(r, 0, 5);
    saveLibrary();
    toast(false, "%s: %d star%s", lib[idx].name.c_str(), r, r == 1 ? "" : "s");
    if (S.pool.rfind("rating:", 0) == 0) rebuildView();
}
void addToSetlist(int idx, const std::string& name) {
    if (idx < 0) return;
    for (auto& s : setlists)
        if (s.name == name) {
            s.items.push_back(lib[idx].path);
            saveLibrary();
            toast(false, "Added to setlist \"%s\" (%d)", name.c_str(), (int)s.items.size());
            if (S.pool == "setlist:" + name) rebuildView();
            return;
        }
    setlists.push_back({name, {lib[idx].path}});
    S.activeSetlist = name;
    saveLibrary();
    toast(false, "Created setlist \"%s\"", name.c_str());
}

// ===================================================================== audio
void refreshDevices() { audioDevices = audio.devices(); }

void startAudioFrom(const std::string& kind, const std::string& device, bool quiet) {
    std::string err;
    bool ok = false;
    if (kind == "none") { audio.stop(); ok = true; }
    else if (kind == "file") ok = audio.startFile(device, err);
    else {
        bool loop = kind == "loopback";
        int idx = -1;
        for (auto& d : audioDevices)
            if (d.loopback == loop && d.name == device) idx = d.index;
        ok = loop ? audio.startLoopback(idx, err) : audio.startInput(idx, err);
    }
    if (ok) {
        S.audioKind = kind;
        S.audioDevice = device;
        saveSettings();
        if (!quiet) toast(false, "%s", audio.label().c_str());
    } else {
        toast(true, "Audio: %s", err.c_str());
    }
}

HWND hwndOf(SDL_Window* w) {
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    SDL_GetWindowWMInfo(w, &wm);
    return wm.info.win.window;
}

void playAudioFileDialog() {
    auto files = plat::pickFiles(hwndOf(win), L"Play an audio file", {{L"Audio", L"*.mp3;*.wav;*.flac;*.ogg"}}, false);
    if (!files.empty()) startAudioFrom("file", files[0], false);
}

void setGain(float g) {
    S.gain = std::clamp(g, 0.05f, 20.f);
    toast(false, "Sensitivity %.2fx", S.gain);
}

bool pumpAudio() {
    static std::vector<float> buf(48000 * 2);
    size_t n = audio.read(buf.data(), 24000);
    double t = now();
    if (n == 0) {
        level *= 0.9f;
        bandBass *= 0.9f; bandMid *= 0.9f; bandTreb *= 0.9f; bandLevel *= 0.9f;
        return false;
    }
    float* s = buf.data();
    static float lp = 0, lp2 = 0;
    double bassE = 0, allE = 0, midE = 0, trebE = 0;
    for (size_t i = 0; i < n; i++) {
        s[2 * i] *= S.gain;
        s[2 * i + 1] *= S.gain;
        float m = 0.5f * (s[2 * i] + s[2 * i + 1]);
        lp += (m - lp) * 0.02f;
        lp2 += (m - lp2) * 0.23f;  // ~2 kHz
        bassE += lp * lp;
        midE += (lp2 - lp) * (lp2 - lp);
        trebE += (m - lp2) * (m - lp2);
        allE += m * m;
    }
    float bass = (float)std::sqrt(bassE / n);
    {
        auto agc = [](float v, float& peak, float& out) {
            peak = std::max(v, std::max(0.01f, peak * 0.9985f));
            out = std::clamp(v / peak, 0.f, 1.f);
        };
        agc(bass, peakBass, bandBass);
        agc((float)std::sqrt(midE / n), peakMid, bandMid);
        agc((float)std::sqrt(trebE / n), peakTreb, bandTreb);
        agc((float)std::sqrt(allE / n), peakLevel, bandLevel);
    }
    level = std::max(level * 0.85f, (float)std::sqrt(allE / n));
    bool hit = bass > 0.03f && bass > bassAvg * 1.35f + 0.01f && t - lastBeatHit > 0.3;
    bassAvg += (bass - bassAvg) * 0.05f;
    if (hit) lastBeatHit = t;
    tempo.feed(s, n, t);
    structure.feed(s, n, t, tempo.beatAt(t), tempo.bpm());
    size_t maxS = projectm_pcm_get_max_samples();
    size_t off = n > maxS ? n - maxS : 0;
    for (auto& d : decks)
        for (auto& p : d.players) p.pushAudio(s + off * 2, n - off);
    thumbs.pushAudio(s, n);
    return hit;
}

// ===================================================================== outputs, spout, recording
void closeOutput(int i) {
    if (i < 0 || i >= (int)outputs.size()) return;
    SDL_DestroyWindow(outputs[i].win);
    outputs.erase(outputs.begin() + i);
    outputSel = std::clamp(outputSel, 0, std::max(0, (int)outputs.size() - 1));
    SDL_GL_MakeCurrent(win, ctx);
    toast(false, "Output closed");
    saveSettings();
}

Output* openOutput(int display, int source = 0, const float* corners = nullptr) {
    int n = SDL_GetNumVideoDisplays();
    int mine = SDL_GetWindowDisplayIndex(win);
    if (display == -2) {
        display = -1;
        for (int i = 0; i < n; i++)
            if (i != mine) { display = i; break; }
    }
    Output o;
    o.display = display;
    o.source = source;
    if (corners) memcpy(o.c, corners, sizeof o.c);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    char title[64];
    snprintf(title, sizeof title, "Amethyst - Output %d", (int)outputs.size() + 1);
    if (display >= 0 && display < n) {
        SDL_Rect b;
        SDL_GetDisplayBounds(display, &b);
        o.win = SDL_CreateWindow(title, b.x, b.y, b.w, b.h, flags | SDL_WINDOW_BORDERLESS);
        toast(false, "Output on %s", SDL_GetDisplayName(display));
    } else {
        o.win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags | SDL_WINDOW_RESIZABLE);
        toast(false, "Output window open — drag it to the projector, press F there for fullscreen");
    }
    SDL_GL_MakeCurrent(win, ctx);
    if (!o.win) { toast(true, "Could not open output: %s", SDL_GetError()); return nullptr; }
    outputs.push_back(o);
    outputSel = (int)outputs.size() - 1;
    saveSettings();
    return &outputs.back();
}

int outputIndexForWindow(Uint32 id) {
    for (size_t i = 0; i < outputs.size(); i++)
        if (SDL_GetWindowID(outputs[i].win) == id) return (int)i;
    return -1;
}

unsigned outputTexture(int source) {
    switch (source) {
        case 1: return decks[0].outTex;
        case 2: return decks[1].outTex;
        case 3: return mixRT.tex;
        default: return finalRT.tex;
    }
}

void toggleFullscreen(SDL_Window* w) {
    bool on = SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(w, on ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void setSpout(bool on) {
    S.spout = on;
    if (!on && spoutNamed) { spout.ReleaseSender(); spoutNamed = false; }
    toast(false, on ? "Spout sender \"Amethyst\" on — add a Spout source in OBS/Resolume" : "Spout off");
}

void startRecording() {
    if (recorder.active()) return;
    std::string dir = plat::videosDir() + "\\Amethyst";
    std::error_code ec;
    fs::create_directories(fs::u8path(dir), ec);
    char name[64];
    std::time_t tt = std::time(nullptr);
    std::strftime(name, sizeof name, "Amethyst-%Y%m%d-%H%M%S.mp4", std::localtime(&tt));
    std::string err;
    if (!recorder.start(dir + "\\" + name, finalRT.w, finalRT.h, S.recFps, err)) { toast(true, "Recording failed: %s", err.c_str()); return; }
    pboPrimed = false;
    recAccum = 0;
    toast(false, "Recording %dx%d @ %d fps -> Videos\\Amethyst", recorder.width(), recorder.height(), S.recFps);
}
void stopRecording() {
    if (!recorder.active()) return;
    std::string path = recorder.path();
    double secs = recorder.seconds();
    int dropped = recorder.dropped();
    recorder.stop();
    toast(false, "Saved %.1fs recording%s", secs, dropped ? " (some frames dropped)" : "");
    fprintf(stderr, "[Amethyst] recording saved: %s\n", path.c_str());
}

RenderTarget recRT;  // fixed recording size, so resizing outputs never interrupts a take

void captureForRecording(double dt) {
    if (!recorder.active()) return;
    recAccum += dt;
    double frame = 1.0 / S.recFps;
    if (recAccum < frame) return;
    int copies = std::min(4, (int)(recAccum / frame));
    recAccum -= copies * frame;
    const int w = recorder.width(), h = recorder.height();
    const size_t size = (size_t)w * h * 4;
    if (!pbo[0]) glGenBuffers(2, pbo);
    recRT.ensure(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, recRT.fbo);
    float a = finalRT.w / (float)std::max(1, finalRT.h), ra = w / (float)h;  // letterbox if aspect changed
    int vw = w, vh = h, vx = 0, vy = 0;
    if (a > ra) { vh = (int)(w / a); vy = (h - vh) / 2; } else { vw = (int)(h * a); vx = (w - vw) / 2; }
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx.present(finalRT.tex, vx, vy, vw, vh, nullptr, false);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, recRT.fbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pboIdx]);
    glBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STREAM_READ);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    if (pboPrimed) {  // map last frame's buffer (already finished) — no stall
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[1 - pboIdx]);
        if (void* p = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT)) {
            for (int c = 0; c < copies; c++) recorder.pushFrame((const unsigned char*)p);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
    }
    pboPrimed = true;
    pboIdx = 1 - pboIdx;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

// ===================================================================== advanced features
void saveScenes();

// ---- modulation routing
enum ModSrc { SrcBass, SrcMid, SrcTreble, SrcLevel, SrcBeat, SrcLfoSine, SrcLfoSaw, SrcLfoSquare, SrcLfoRandom, SrcCount };
const char* kModSrcNames[] = {"Bass", "Mids", "Treble", "Level", "Beat pulse", "LFO sine", "LFO saw", "LFO square", "LFO random"};
enum ModDst { DstZoom, DstRotate, DstSwirl, DstHue, DstSat, DstContrast, DstBright, DstRgb, DstFeedback, DstPixel, DstKaleido,
              DstCrossfade, DstVideoOpacity, DstLogoOpacity, DstTextOpacity, DstCount };
const char* kModDstNames[] = {"Zoom", "Rotate", "Swirl", "Hue", "Saturation", "Contrast", "Brightness", "RGB split", "Feedback trails",
                              "Pixelate", "Kaleidoscope segments", "Crossfader", "Video opacity", "Logo opacity", "Text opacity"};
struct ModRoute {
    int src = SrcBass, dst = DstZoom;
    float amount = 0.3f, rate = 4, smooth = 0.5f;  // rate = LFO period in beats
    bool on = true;
    float state = 0;
};
std::vector<ModRoute> mods;
float modSource(int src, double beat) {
    auto frac = [](double x) { return (float)(x - std::floor(x)); };
    switch (src) {
        case SrcBass: return bandBass;
        case SrcMid: return bandMid;
        case SrcTreble: return bandTreb;
        case SrcLevel: return bandLevel;
        case SrcBeat: { float f = 1 - frac(beat); return f * f; }
        default: break;
    }
    return 0;
}
float lfo(int src, double beat, float rate) {
    double ph = beat / std::max(0.0625f, rate);
    float f = (float)(ph - std::floor(ph));
    switch (src) {
        case SrcLfoSine: return 0.5f + 0.5f * std::sin(f * 6.2831853f);
        case SrcLfoSaw: return f;
        case SrcLfoSquare: return f < 0.5f ? 1.f : 0.f;
        case SrcLfoRandom: { double k = std::floor(ph); return (float)(std::fmod(std::sin(k * 12.9898) * 43758.5453, 1.0) + 1) * 0.5f; }
        default: return 0;
    }
}
struct ModOut { FxParams fx; float xfade, rotOffset, videoOpacity, logoOpacity, textOpacity; } modOut;
void applyModulation(double beat) {
    ModOut m{S.fx, S.xfade, 0, S.videoOpacity, S.logoOpacity, S.textOpacity};
    for (auto& r : mods) {
        if (!r.on) { r.state = 0; continue; }
        float v = r.src >= SrcLfoSine ? lfo(r.src, beat, r.rate) : modSource(r.src, beat);
        r.state += (v - r.state) * (1 - std::clamp(r.smooth, 0.f, 0.97f));
        float a = r.amount * r.state;
        FxParams& f = m.fx;
        switch (r.dst) {
            case DstZoom: f.zoom *= std::max(0.1f, 1 + a); break;
            case DstRotate: m.rotOffset += a * 3.14159f; break;
            case DstSwirl: f.swirl += a * 3; break;
            case DstHue: f.hue = f.hue + a - std::floor(f.hue + a); break;
            case DstSat: f.saturation = std::max(0.f, f.saturation * (1 + a * 1.5f)); break;
            case DstContrast: f.contrast = std::max(0.1f, f.contrast * (1 + a)); break;
            case DstBright: f.brightness += a * 0.5f; break;
            case DstRgb: if (a > 0.01f) { f.rgbSplitAmount = (f.rgbSplit ? f.rgbSplitAmount : 0) + a * 0.03f; f.rgbSplit = true; } break;
            case DstFeedback: if (f.feedback || a > 0.02f) { f.feedbackAmount = std::clamp((f.feedback ? f.feedbackAmount : 0) + a, 0.f, 0.97f); f.feedback = true; } break;
            case DstPixel: if (a > 0.05f) { f.pixelSize = (f.pixelate ? f.pixelSize : 2) + a * 40; f.pixelate = true; } break;
            case DstKaleido: if (a > 0.08f) { f.kaleidoSegments = std::clamp((f.kaleido ? f.kaleidoSegments : 2) + (int)std::round(a * 10), 2, 24); f.kaleido = true; } break;
            case DstCrossfade: m.xfade = std::clamp(m.xfade + a, 0.f, 1.f); break;
            case DstVideoOpacity: m.videoOpacity = std::clamp(m.videoOpacity + a, 0.f, 1.f); break;
            case DstLogoOpacity: m.logoOpacity = std::clamp(m.logoOpacity + a, 0.f, 1.f); break;
            case DstTextOpacity: m.textOpacity = std::clamp(m.textOpacity + a, 0.f, 1.f); break;
        }
    }
    modOut = m;
}

// ---- scenes (whole looks) with morphing, and a beat-based show timeline
struct Scene {
    bool used = false;
    std::string name, presetA, presetB, text;
    float xfade = 0, videoOpacity = 0.6f;
    int mixMode = 0;
    bool textOn = false, logoOn = false, videoOn = false;
    FxParams fx;
};
constexpr int kScenes = 16;
Scene scenes[kScenes];
int activeScene = -1;
struct Morph { bool active = false; FxParams from, to; float xFrom = 0, xTo = 0; double startBeat = 0, beats = 0; } morph;
struct TlStep { int scene = 0; int bars = 4; };
std::vector<TlStep> timeline;
bool tlPlaying = false, tlLoop = true;
int tlIndex = 0;
double tlStepStart = 0;

std::vector<Field> fxFields(FxParams& f) {
    return {{"kaleido", 'b', &f.kaleido}, {"kaleido_n", 'i', &f.kaleidoSegments}, {"mirror", 'b', &f.mirror}, {"mirror_mode", 'i', &f.mirrorMode},
            {"feedback", 'b', &f.feedback}, {"feedback_amt", 'f', &f.feedbackAmount}, {"feedback_zoom", 'f', &f.feedbackZoom},
            {"feedback_rot", 'f', &f.feedbackRotate}, {"rgb", 'b', &f.rgbSplit}, {"rgb_amt", 'f', &f.rgbSplitAmount}, {"pixel", 'b', &f.pixelate},
            {"pixel_size", 'f', &f.pixelSize}, {"invert", 'b', &f.invert}, {"strobe", 'b', &f.strobe}, {"strobe_rate", 'f', &f.strobeRate},
            {"strobe_beat", 'b', &f.strobeOnBeat}, {"zoom", 'f', &f.zoom}, {"rotate", 'f', &f.rotateSpeed}, {"swirl", 'f', &f.swirl},
            {"hue", 'f', &f.hue}, {"sat", 'f', &f.saturation}, {"contrast", 'f', &f.contrast}, {"bright", 'f', &f.brightness}};
}

FxParams lerpFx(const FxParams& a, const FxParams& b, float k) {
    FxParams r = b;  // switches take the destination value immediately
    auto L = [k](float x, float y) { return x + (y - x) * k; };
    r.kaleidoSegments = (int)std::round(L((float)a.kaleidoSegments, (float)b.kaleidoSegments));
    r.feedbackAmount = L(a.feedbackAmount, b.feedbackAmount);
    r.feedbackZoom = L(a.feedbackZoom, b.feedbackZoom);
    r.feedbackRotate = L(a.feedbackRotate, b.feedbackRotate);
    r.rgbSplitAmount = L(a.rgbSplitAmount, b.rgbSplitAmount);
    r.pixelSize = L(a.pixelSize, b.pixelSize);
    r.strobeRate = L(a.strobeRate, b.strobeRate);
    r.zoom = L(a.zoom, b.zoom);
    r.rotateSpeed = L(a.rotateSpeed, b.rotateSpeed);
    r.swirl = L(a.swirl, b.swirl);
    float dh = b.hue - a.hue;  // shortest way round the colour wheel
    if (dh > 0.5f) dh -= 1;
    if (dh < -0.5f) dh += 1;
    r.hue = a.hue + dh * k - std::floor(a.hue + dh * k);
    r.saturation = L(a.saturation, b.saturation);
    r.contrast = L(a.contrast, b.contrast);
    r.brightness = L(a.brightness, b.brightness);
    return r;
}

void storeScene(int i) {
    if (i < 0 || i >= kScenes) return;
    Scene& s = scenes[i];
    s.used = true;
    if (s.name.empty()) s.name = "Scene " + std::to_string(i + 1);
    s.presetA = decks[0].cur >= 0 ? lib[decks[0].cur].path : "";
    s.presetB = decks[1].cur >= 0 ? lib[decks[1].cur].path : "";
    s.xfade = S.xfade;
    s.mixMode = S.mixMode;
    s.fx = S.fx;
    s.textOn = S.textOn;
    s.text = S.text;
    s.logoOn = S.logoOn;
    s.videoOn = S.videoOn;
    s.videoOpacity = S.videoOpacity;
    activeScene = i;
    saveScenes();
    toast(false, "Stored %s", s.name.c_str());
}

void recallScene(int i) {
    if (i < 0 || i >= kScenes || !scenes[i].used) { toast(false, "Scene %d is empty — store one with Ctrl+Shift+F%d or in the Scenes panel", i + 1, i + 1); return; }
    Scene& s = scenes[i];
    for (int d = 0; d < 2; d++) {
        auto it = byPath.find(d ? s.presetB : s.presetA);
        if (it != byPath.end() && it->second != decks[d].cur) loadPreset(d, it->second, false);
    }
    S.mixMode = s.mixMode;
    S.textOn = s.textOn;
    S.text = s.text;
    S.logoOn = s.logoOn;
    S.videoOn = s.videoOn;
    S.videoOpacity = s.videoOpacity;
    if (S.sceneMorphBeats <= 0.01f) {
        S.fx = s.fx;
        S.xfade = s.xfade;
        morph.active = false;
    } else {
        morph = {true, S.fx, s.fx, S.xfade, s.xfade, beatNow(), S.sceneMorphBeats};
    }
    xanim.active = false;
    activeScene = i;
    toast(false, "%s%s", s.name.c_str(), S.sceneMorphBeats > 0.01f ? "  (morphing)" : "");
}

void updateMorph() {
    if (!morph.active) return;
    float k = (float)std::clamp((beatNow() - morph.startBeat) / std::max(0.01, morph.beats), 0.0, 1.0);
    float e = k * k * (3 - 2 * k);
    S.fx = lerpFx(morph.from, morph.to, e);
    S.xfade = morph.xFrom + (morph.xTo - morph.xFrom) * e;
    if (k >= 1) { S.fx = morph.to; morph.active = false; }
}

void timelinePlay(bool on) {
    tlPlaying = on && !timeline.empty();
    if (tlPlaying) {
        tlIndex = 0;
        tlStepStart = std::floor(beatNow() / 4) * 4;
        recallScene(timeline[0].scene);
        toast(false, "Show timeline playing (%d steps)", (int)timeline.size());
    } else toast(false, "Show timeline stopped");
}

void updateTimeline() {
    if (!tlPlaying || timeline.empty()) return;
    double beat = beatNow();
    double len = std::max(1, timeline[tlIndex].bars) * 4.0;
    if (beat < tlStepStart + len) return;
    tlStepStart += len;
    if (beat > tlStepStart + 16) tlStepStart = std::floor(beat / 4) * 4;  // clock jumped: resync to the bar
    tlIndex++;
    if (tlIndex >= (int)timeline.size()) {
        if (!tlLoop) { tlPlaying = false; tlIndex = 0; toast(false, "Show timeline finished"); return; }
        tlIndex = 0;
    }
    recallScene(timeline[tlIndex].scene);
}

void saveScenes() {
    std::ofstream f(fs::u8path(gScenesPath), std::ios::trunc);
    if (!f) return;
    for (int i = 0; i < kScenes; i++) {
        Scene& s = scenes[i];
        if (!s.used) continue;
        f << "scene=" << i << "|" << s.name << "|" << s.presetA << "|" << s.presetB << "|" << s.xfade << "|" << s.mixMode << "|" << s.textOn << "|"
          << s.logoOn << "|" << s.videoOn << "|" << s.videoOpacity << "|" << s.text << "\n";
        f << "scenefx=" << i;
        for (auto& fl : fxFields(s.fx)) {
            f << "|" << fl.key << ":";
            if (fl.type == 'f') f << *(float*)fl.p; else if (fl.type == 'i') f << *(int*)fl.p; else f << (*(bool*)fl.p ? 1 : 0);
        }
        f << "\n";
    }
    for (auto& st : timeline) f << "step=" << st.scene << "|" << st.bars << "\n";
    f << "timeline_loop=" << (tlLoop ? 1 : 0) << "\n";
    for (auto& m : mods) f << "mod=" << m.src << "|" << m.dst << "|" << m.amount << "|" << m.rate << "|" << m.smooth << "|" << (m.on ? 1 : 0) << "\n";
}

void loadScenes() {
    std::ifstream f(fs::u8path(gScenesPath));
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto p = splitBar(v);
        if (k == "scene" && p.size() >= 11) {
            int i = std::atoi(p[0].c_str());
            if (i < 0 || i >= kScenes) continue;
            Scene& s = scenes[i];
            s.used = true;
            s.name = p[1]; s.presetA = p[2]; s.presetB = p[3];
            s.xfade = (float)std::atof(p[4].c_str()); s.mixMode = std::atoi(p[5].c_str());
            s.textOn = p[6] == "1"; s.logoOn = p[7] == "1"; s.videoOn = p[8] == "1";
            s.videoOpacity = (float)std::atof(p[9].c_str());
            s.text = p[10];
            for (size_t x = 11; x < p.size(); x++) s.text += "|" + p[x];
        } else if (k == "scenefx" && !p.empty()) {
            int i = std::atoi(p[0].c_str());
            if (i < 0 || i >= kScenes) continue;
            auto ff = fxFields(scenes[i].fx);
            for (size_t x = 1; x < p.size(); x++) {
                auto c = p[x].find(':');
                if (c == std::string::npos) continue;
                std::string key = p[x].substr(0, c), val = p[x].substr(c + 1);
                for (auto& fl : ff)
                    if (key == fl.key) {
                        if (fl.type == 'f') *(float*)fl.p = (float)std::atof(val.c_str());
                        else if (fl.type == 'i') *(int*)fl.p = std::atoi(val.c_str());
                        else *(bool*)fl.p = val == "1";
                    }
            }
        } else if (k == "step" && p.size() == 2) timeline.push_back({std::clamp(std::atoi(p[0].c_str()), 0, kScenes - 1), std::max(1, std::atoi(p[1].c_str()))});
        else if (k == "timeline_loop") tlLoop = v == "1";
        else if (k == "mod" && p.size() == 6) {
            ModRoute m;
            m.src = std::clamp(std::atoi(p[0].c_str()), 0, SrcCount - 1);
            m.dst = std::clamp(std::atoi(p[1].c_str()), 0, DstCount - 1);
            m.amount = (float)std::atof(p[2].c_str());
            m.rate = (float)std::atof(p[3].c_str());
            m.smooth = (float)std::atof(p[4].c_str());
            m.on = p[5] == "1";
            mods.push_back(m);
        }
    }
}

// ---- deck transitions (used whenever the crossfader is animated)
double transitionSeconds() {
    if (S.transBeats > 0.01f) return S.transBeats * 60.0 / std::max(30.0, tempo.bpm());
    return std::max(0.5f, S.blend);
}

// ---- live input layer: Spout receive
SpoutReceiver spoutRx;
GLuint spoutRxTex = 0;
int spoutRxW = 0, spoutRxH = 0;
std::string spoutRxConnected;
bool receiveSpout() {
    if (S.spoutInput.empty()) spoutRx.SetReceiverName(nullptr);
    else spoutRx.SetReceiverName(S.spoutInput.c_str());
    if (!spoutRxTex) {
        glGenTextures(1, &spoutRxTex);
        glBindTexture(GL_TEXTURE_2D, spoutRxTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if (!spoutRx.ReceiveTexture(spoutRxTex, GL_TEXTURE_2D, false, 0)) return false;
    if (spoutRx.IsUpdated()) {
        spoutRxW = (int)spoutRx.GetSenderWidth();
        spoutRxH = (int)spoutRx.GetSenderHeight();
        glBindTexture(GL_TEXTURE_2D, spoutRxTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, spoutRxW, spoutRxH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        spoutRxConnected = spoutRx.GetSenderName();
        return false;  // texture re-sized; next frame receives into it
    }
    return spoutRxW > 0;
}
std::vector<std::string> spoutSenders() {
    std::vector<std::string> out;
    int n = spoutRx.GetSenderCount();
    for (int i = 0; i < n; i++) {
        char name[256];
        if (spoutRx.GetSender(i, name, sizeof name)) out.push_back(name);
    }
    return out;
}
void openLayerSource() {
    std::string err;
    if (S.layerSource == 1) {
        if (!video.openCamera(S.cameraIndex, err)) toast(true, "Camera: %s", err.c_str());
        else toast(false, "Camera layer: %s", video.name().c_str());
    } else if (S.layerSource == 0 && !S.videoPath.empty()) {
        if (!video.open(S.videoPath, err)) toast(true, "Video: %s", err.c_str());
    } else {
        video.close();
    }
    videoSerial = -1;
}

// ---- Art-Net / DMX lighting
ArtNet artnet;
RenderTarget dmxRT;
double lastDmx = 0;
std::vector<std::array<unsigned char, 3>> dmxColors;
unsigned char dmxData[512];
const char* kDmxModes[] = {"RGB (3 ch)", "RGBW (4 ch)", "Dimmer + RGB (4 ch)", "Dimmer + RGB + Strobe (5 ch)"};
const int kDmxWidth[] = {3, 4, 4, 5};
void sendDmx(double t, float strobeWhite) {
    if (!S.dmxOn || t - lastDmx < 1.0 / 40) return;
    lastDmx = t;
    int n = std::clamp(S.dmxFixtures, 1, 64);
    dmxRT.ensure(n, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, dmxRT.fbo);
    gfx.present(finalRT.tex, 0, 0, n, 1, nullptr, false);
    std::vector<unsigned char> px((size_t)n * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, n, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    dmxColors.resize(n);
    memset(dmxData, 0, sizeof dmxData);
    int w = kDmxWidth[std::clamp(S.dmxMode, 0, 3)];
    float lvl = std::clamp(S.dmxLevel, 0.f, 1.f);
    for (int i = 0; i < n; i++) {
        unsigned char r = (unsigned char)(px[i * 4] * lvl), g = (unsigned char)(px[i * 4 + 1] * lvl), b = (unsigned char)(px[i * 4 + 2] * lvl);
        dmxColors[i] = {r, g, b};
        int base = S.dmxStart - 1 + i * w;
        if (base < 0 || base + w > 512) break;
        unsigned char* c = dmxData + base;
        switch (S.dmxMode) {
            case 0: c[0] = r; c[1] = g; c[2] = b; break;
            case 1: { unsigned char wv = std::min(r, std::min(g, b)); c[0] = r - wv; c[1] = g - wv; c[2] = b - wv; c[3] = wv; break; }
            case 2: c[0] = 255; c[1] = r; c[2] = g; c[3] = b; break;
            default: c[0] = 255; c[1] = r; c[2] = g; c[3] = b; c[4] = strobeWhite > 0.5f ? 255 : 0; break;
        }
    }
    artnet.send(S.dmxIp, S.dmxUniverse, dmxData, std::min(512, S.dmxStart - 1 + n * w));
}

// ---- DJ link (Denon StageLinQ)
StageLinq djLink;
StageLinq::Snapshot djSnap;
std::string djLastTitle;
double djTitleUntil = 0;
std::string djTitleText;
void setDjLink(bool on) {
    S.djOn = on;
    if (on) djLink.start(); else { djLink.stop(); if (tempo.source() == Tempo::Source::External) tempo.setSource(Tempo::Source::Auto); }
    toast(false, on ? "DJ link on — searching for Engine OS devices" : "DJ link off");
}
void updateDjLink(double t) {
    if (!S.djOn) return;
    djSnap = djLink.snapshot();
    int on = StageLinq::onAirDeck(djSnap);
    if (on < 0) return;
    const DjDeck& d = djSnap.decks[on];
    if (S.djTempo && d.bpm > 0) {
        double beat = d.hasBeat ? d.beat + (t - d.beatTime) * d.bpm / 60.0 : tempo.beatAt(t);
        tempo.setExternal(d.bpm, beat, t);
    }
    if (!d.title.empty() && d.title != djLastTitle) {
        bool first = djLastTitle.empty();
        djLastTitle = d.title;
        djTitleText = d.artist.empty() ? d.title : d.artist + "  -  " + d.title;
        if (!first) structure.trackChanged(t, tempo.beatAt(t));
        if (S.djShowTitle) djTitleUntil = t + S.djTitleSecs;
        if (!first) {
            switch (S.djTrackAction) {
                case 1: go(focus, 1, false); break;
                case 2: go(focus, 1, false, true); break;
                case 3: {
                    int next = activeScene;
                    for (int k = 1; k <= kScenes; k++) {
                        int c = (activeScene + k + kScenes) % kScenes;
                        if (scenes[c].used) { next = c; break; }
                    }
                    if (next >= 0) recallScene(next);
                    break;
                }
                default: break;
            }
        }
        toast(false, "Now playing: %s", djTitleText.c_str());
    }
}

// ---- MIDI LED feedback + controller templates
std::map<std::string, int> ledCache;
double lastLed = 0;
int paletteFor(const std::string& group, bool bright) {
    static const std::map<std::string, std::pair<int, int>> pal = {
        {"Presets", {53, 55}}, {"Cues", {9, 11}}, {"Mixer", {37, 39}}, {"FX", {21, 23}}, {"Performance", {5, 7}}, {"Auto-pilot", {45, 47}},
        {"Tempo", {57, 59}}, {"Output", {3, 1}}, {"Scenes", {13, 15}}, {"Overlays", {3, 1}}, {"Library", {13, 15}}, {"View", {1, 1}}};
    auto it = pal.find(group);
    auto p = it == pal.end() ? std::make_pair(3, 1) : it->second;
    return bright ? p.first : p.second;
}
void updateLeds(double t, bool force = false) {
    if (!force && t - lastLed < 0.05) return;
    lastLed = t;
    for (auto& b : midi.bindings) {
        Action* a = actions.find(b.action);
        if (!a || a->kind == Action::Continuous) continue;
        int style = midi.feedbackStyle.count(b.device) ? midi.feedbackStyle[b.device] : 0;
        int level;  // 0 off, 1 dim (available), 2 bright (on/active)
        if (a->id.rfind("cue", 0) == 0 && a->id.size() == 4) {
            int slot = (a->id[3] - '0' + 9) % 10;
            auto it = S.cues[slot].empty() ? byPath.end() : byPath.find(S.cues[slot]);
            level = it == byPath.end() ? 0 : (it->second == decks[0].cur || it->second == decks[1].cur) ? 2 : 1;
        } else if (a->id.rfind("scene", 0) == 0 && a->id.size() > 5 && isdigit((unsigned char)a->id[5])) {
            int i = std::atoi(a->id.c_str() + 5) - 1;
            level = i < 0 || i >= kScenes || !scenes[i].used ? 0 : i == activeScene ? 2 : 1;
        } else if (a->get) level = a->get() > 0.5f ? 2 : 1;
        else level = 1;
        int vel = style >= 1 ? (level == 0 ? 0 : paletteFor(a->group, level == 2)) : (level == 2 ? 127 : 0);
        std::string key = b.device + "|" + std::to_string(b.type) + "|" + std::to_string(b.channel) + "|" + std::to_string(b.number);
        auto c = ledCache.find(key);
        if (!force && c != ledCache.end() && c->second == vel) continue;
        ledCache[key] = vel;
        midi.send(b.device, (unsigned char)((b.type ? 0xB0 : 0x90) | b.channel), (unsigned char)b.number, (unsigned char)vel);
    }
}

// Launchpad programmer mode SysEx per model
std::vector<unsigned char> launchpadSysex(int model) {
    unsigned char id = model == 1 ? 0x0D : model == 2 ? 0x0C : 0x0E;  // Mini MK3, X, Pro MK3
    return {0xF0, 0x00, 0x20, 0x29, 0x02, id, 0x0E, 0x01, 0xF7};
}
const char* kTemplates[] = {"Novation Launchpad Mini MK3", "Novation Launchpad X", "Novation Launchpad Pro MK3", "Akai APC40 mkII"};
void applyTemplate(int tpl, const std::string& device) {
    midi.bindings.erase(std::remove_if(midi.bindings.begin(), midi.bindings.end(), [&](const MidiBinding& b) { return b.device == device; }), midi.bindings.end());
    auto note = [&](int n, const char* id, int ch = 0) { midi.bindings.push_back({id, device, 0, ch, n}); };
    auto cc = [&](int n, const char* id, int ch = 0) { midi.bindings.push_back({id, device, 1, ch, n}); };
    const char* rows[5][8] = {
        {"cue1", "cue2", "cue3", "cue4", "cue5", "cue6", "cue7", "cue8"},
        {"cue9", "cue0", "scene1", "scene2", "scene3", "scene4", "scene5", "scene6"},
        {"fx_kaleido", "fx_mirror", "fx_feedback", "fx_rgb", "fx_pixel", "fx_invert", "fx_strobe", "blackout"},
        {"prev", "next", "random", "auto", "shuffle", "quantize", "tap", "flash"},
        {"a_next", "a_random", "b_next", "b_random", "focus_toggle", "xfade_auto", "mix_mode", "lock"}};
    if (tpl <= 2) {  // Launchpad programmer mode: note = row*10 + col, row 8 at the top
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 8; c++) note((8 - r) * 10 + c + 1, rows[r][c]);
        const char* row3[8] = {"scene7", "scene8", "timeline_play", "record", "spout", "logo", "text", "video"};
        for (int c = 0; c < 8; c++) note(30 + c + 1, row3[c]);
        cc(89, "xfade_left"); cc(79, "xfade_right"); cc(69, "speed_down"); cc(59, "speed_up"); cc(49, "transition");
        midi.feedbackStyle[device] = 2 + tpl;
        midi.sendSysex(device, launchpadSysex(tpl + 1));
    } else {  // APC40 mkII: clip grid notes 0..39 (row 0 at the bottom), ch 1
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 8; c++) note((4 - r) * 8 + c, rows[r][c]);
        const char* sceneBtns[5] = {"scene7", "scene8", "timeline_play", "record", "spout"};
        for (int i = 0; i < 5; i++) note(82 + i, sceneBtns[i]);
        note(99, "tap");
        cc(15, "crossfade");
        cc(14, "gain");
        const char* knobs[8] = {"fx_hue", "fx_zoom", "fx_rotate", "fx_swirl", "fx_feedback_amt", "speed", "video_opacity", "blend"};
        for (int i = 0; i < 8; i++) cc(16 + i, knobs[i]);
        midi.feedbackStyle[device] = 1;
    }
    ledCache.clear();
    saveSettings();
    toast(false, "Applied %s template to %s (%d controls)", kTemplates[tpl], device.c_str(), (int)std::count_if(midi.bindings.begin(), midi.bindings.end(), [&](const MidiBinding& b) { return b.device == device; }));
}

// ---- smart library: analysis results, colour/energy pools, similarity
struct Look { float hue = 0, sat = 0, bright = 0, motion = 0; };
std::unordered_map<std::string, Look> looks;
bool looksDirty = false;
double lastLookSave = 0;
const char* kColorNames[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink", "Mono"};
const char* colorOf(const Look& l) {
    if (l.sat < 0.18f) return "Mono";
    float h = l.hue;
    if (h < 0.04f || h >= 0.94f) return "Red";
    if (h < 0.11f) return "Orange";
    if (h < 0.19f) return "Yellow";
    if (h < 0.45f) return "Green";
    if (h < 0.55f) return "Cyan";
    if (h < 0.72f) return "Blue";
    if (h < 0.83f) return "Purple";
    return "Pink";
}
float lookDistance(const Look& a, const Look& b) {
    float dh = std::fabs(a.hue - b.hue);
    dh = std::min(dh, 1 - dh) * 2 * std::min(a.sat, b.sat) * 2;  // hue matters only for vivid presets
    float ds = a.sat - b.sat, db = a.bright - b.bright, dm = a.motion - b.motion;
    return dh * dh + ds * ds + db * db * 1.5f + dm * dm * 2;
}
void saveLooks() {
    std::ofstream f(fs::u8path(gAnalysisPath), std::ios::trunc);
    for (auto& [p, l] : looks) f << p << "|" << l.hue << "|" << l.sat << "|" << l.bright << "|" << l.motion << "\n";
    looksDirty = false;
}
void loadLooks() {
    std::ifstream f(fs::u8path(gAnalysisPath));
    std::string line;
    while (std::getline(f, line)) {
        auto p = splitBar(line);
        if (p.size() != 5) continue;
        looks[p[0]] = {(float)std::atof(p[1].c_str()), (float)std::atof(p[2].c_str()), (float)std::atof(p[3].c_str()), (float)std::atof(p[4].c_str())};
    }
}
void queueAnalysis() {
    std::vector<std::string> todo;
    for (auto& p : lib)
        if (!p.broken && !looks.count(p.path)) todo.push_back(p.path);
    thumbs.analysisEnabled = S.analysisOn;
    thumbs.queueAnalysis(std::move(todo));
}
void intakeAnalysis(double t) {
    for (auto& a : thumbs.takeAnalysis()) {
        if (a.failed) {
            auto it = byPath.find(a.path);
            if (it != byPath.end()) markBroken(it->second, "failed during analysis");
            continue;
        }
        looks[a.path] = {a.hue, a.sat, a.bright, a.motion};
        looksDirty = true;
    }
    if (looksDirty && t - lastLookSave > 20) { lastLookSave = t; saveLooks(); }
}
// pools understood by inPool(): "color:<Name>", "energy:calm|intense", "light:dark|bright", "similar:<path>"
bool smartPoolMatch(int i) {
    auto it = looks.find(lib[i].path);
    if (it == looks.end()) return false;
    const Look& l = it->second;
    if (S.pool.rfind("color:", 0) == 0) return S.pool.substr(6) == colorOf(l);
    if (S.pool == "energy:calm") return l.motion < 0.12f;
    if (S.pool == "energy:intense") return l.motion > 0.35f;
    if (S.pool == "light:dark") return l.bright < 0.18f;
    if (S.pool == "light:bright") return l.bright > 0.42f;
    return true;
}


// ---- smart auto-mix: follows the song structure, prepares the next preset ahead of time and
// switches exactly on the bar / drop.
struct Smart {
    bool prep = false;
    int deck = -1, idx = -1;
    int fireOn = 0;  // 0 = at fireBeat, 1 = on the next drop, 2 = when the next song starts
    double fireBeat = 0, blend = 0;
    bool hard = false;
    double lastSwitch = 0;
    bool afterFake = false;  // between a fakeout and the real drop
    std::string lastEvent;
    double lastEventAt = -100;
} smart;

int onAirDeck() { return S.xfade < 0.5f ? 0 : 1; }

int smartPick(int energy) {
    int on = onAirDeck();
    if (energy == 0) return S.shuffle ? pickRandom(decks[on].cur) : stepFrom(decks[on].cur, 1);
    std::vector<int> c;
    for (int i : view) {
        if (lib[i].broken || i == decks[0].cur || i == decks[1].cur) continue;
        auto it = looks.find(lib[i].path);
        if (it == looks.end()) continue;
        float m = it->second.motion;
        if (energy > 0 ? m > 0.35f : m < 0.12f) c.push_back(i);
    }
    if (c.empty()) return pickRandom(decks[on].cur);
    return c[std::uniform_int_distribution<size_t>(0, c.size() - 1)(rng)];
}

bool releaseHeld(int d, double blendSecs) {
    Deck& dk = decks[d];
    if (dk.heldIdx < 0) return false;
    int idx = dk.heldIdx;
    dk.heldIdx = -1;
    if (dk.cur >= 0 && dk.cur != idx) {
        dk.hist.push_back(lib[dk.cur].path);
        if (dk.hist.size() > 300) dk.hist.erase(dk.hist.begin());
    }
    dk.cur = idx;
    dk.lastSwitch = now();
    (d ? S.lastB : S.lastA) = lib[idx].path;
    dk.active = 1 - dk.active;
    dk.blending = blendSecs > 0.01;
    dk.blendStart = now();
    dk.blendDur = blendSecs;
    return true;
}

void smartCancel() {
    if (smart.prep && smart.deck >= 0) {
        Deck& dk = decks[smart.deck];
        if (dk.heldIdx == smart.idx) dk.heldIdx = -1;
        if (dk.pendingIdx == smart.idx && dk.pendingHold) { dk.pendingIdx = -1; dk.pendingToken = 0; dk.pendingHold = false; }
    }
    smart.prep = false;
}
bool smartValid() {
    Deck& dk = decks[smart.deck];
    if (S.autoDecks) return dk.pendingIdx == smart.idx || dk.cur == smart.idx;
    return (dk.pendingIdx == smart.idx && dk.pendingHold) || dk.heldIdx == smart.idx;
}
bool smartReady() {
    Deck& dk = decks[smart.deck];
    return S.autoDecks ? dk.pendingIdx < 0 && dk.cur == smart.idx : dk.heldIdx == smart.idx;
}
bool smartPrepare(int energy, int fireOn, double fireBeat, double blend, bool hard) {
    smartCancel();
    int on = onAirDeck(), d = S.autoDecks ? 1 - on : on;
    if (S.autoDecks ? xanim.active : decks[d].blending) return false;  // the spare is still in use
    int idx = smartPick(energy);
    if (idx < 0 || !loadPreset(d, idx, true)) return false;
    if (!S.autoDecks) decks[d].pendingHold = true;
    smart.prep = true;
    smart.deck = d, smart.idx = idx, smart.fireOn = fireOn, smart.fireBeat = fireBeat, smart.blend = blend, smart.hard = hard;
    return true;
}
void smartFire(double t) {
    if (S.autoDecks) startXfade(smart.deck ? 1.f : 0.f, smart.hard ? 0.04 : smart.blend);
    else releaseHeld(smart.deck, smart.hard ? 0 : smart.blend);
    smart.prep = false;
    smart.lastSwitch = t;
}
double barSeconds() { return 240.0 / std::max(30.0, tempo.bpm()); }
int phraseBars() {
    double bars = S.interval / barSeconds();
    return bars >= 8 ? 8 : bars >= 4 ? 4 : bars >= 2 ? 2 : 1;
}

void smartTick(double t) {
    double beat = beatNow(), lead = 1.5 / barSeconds() * 4;  // ~1.5 s for the preset to compile
    auto events = structure.take();
    bool active = S.autoOn && S.smartMix && !locked;
    for (auto& e : events) {
        smart.lastEvent = MusicStructure::name(e.type);
        smart.lastEventAt = t;
        if (!active) continue;
        double nextBar = structure.nextBoundary(beat + lead, 1);
        switch (e.type) {
            case MusicStructure::DropHit:
                if (smart.afterFake && !S.fakeoutRecut) { smart.afterFake = false; break; }  // keep the fake-drop preset
                smart.afterFake = false;
                if (smart.prep && smart.fireOn == 1 && smartReady()) smartFire(t);
                else if (smartPrepare(1, 0, beat, 0, true)) smart.fireBeat = beat;  // cut as soon as it compiles
                break;
            case MusicStructure::BreakdownStart: smartPrepare(-1, 0, nextBar, barSeconds() * 4, false); break;
            case MusicStructure::SongStart:
                if (smart.prep && smart.fireOn == 2 && smartReady()) smartFire(t);
                else smartPrepare(0, 0, nextBar, transitionSeconds(), false);
                break;
            case MusicStructure::SongChange: smartPrepare(0, 0, nextBar, transitionSeconds(), false); break;
            case MusicStructure::SongEnd: smartPrepare(0, 2, 0, 0, true); break;
            case MusicStructure::Fakeout:  // that was not the real drop: get another one ready for it
                smart.afterFake = true;
                if (S.fakeoutRecut) smartPrepare(1, 1, 0, 0, true);
                break;
        }
    }
    if (!active) {
        if (smart.prep) smartCancel();
        return;
    }
    if (smart.prep && !smartValid()) smart.prep = false;  // failed to load, or the user took over the deck
    auto sec = structure.section();
    bool low = sec == MusicStructure::Breakdown || sec == MusicStructure::Build;
    if (smart.prep) {
        if (smart.fireOn == 0 && beat >= smart.fireBeat - 0.02) {
            if (smartReady()) smartFire(t);
            else smart.fireBeat = structure.nextBoundary(beat, 1);  // still compiling: next bar
        } else if (smart.fireOn == 1 && !low) {  // breakdown ended without a clear drop
            smart.fireOn = 0, smart.hard = false, smart.blend = transitionSeconds();
            smart.fireBeat = structure.nextBoundary(beat, 1);
        }
        return;
    }
    double since = t - std::max(smart.lastSwitch, decks[onAirDeck()].lastSwitch);
    if (low) {
        if (since > 2 && !(smart.afterFake && !S.fakeoutRecut)) smartPrepare(1, 1, 0, 0, true);  // have the drop preset ready
    } else if (sec != MusicStructure::Silence && since >= S.interval - 2) {
        smartPrepare(0, 0, structure.nextBoundary(beat + lead, phraseBars()), transitionSeconds(), false);
    }
}

void sortSimilar(std::vector<int>& v) {
    std::string refPath = S.pool.substr(8);
    auto ref = looks.find(refPath);
    if (ref == looks.end()) return;
    const Look r = ref->second;
    std::vector<std::pair<float, int>> d;
    for (int i : v) {
        auto it = looks.find(lib[i].path);
        if (it != looks.end()) d.push_back({lib[i].path == refPath ? -1.f : lookDistance(r, it->second), i});
    }
    std::sort(d.begin(), d.end());
    v.clear();
    for (size_t k = 0; k < d.size() && k < 80; k++) v.push_back(d[k].second);
}

// ===================================================================== overlays
void loadLogo() {
    if (S.logoPath == loadedLogoPath) return;
    loadedLogoPath = S.logoPath;
    if (S.logoPath.empty()) return;
    std::ifstream f(fs::u8path(S.logoPath), std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)), {});
    unsigned char* px = data.empty() ? nullptr : imageDecodeRGBA(data.data(), (int)data.size(), &logoW, &logoH);
    if (!px) { toast(true, "Could not load image %s", S.logoPath.c_str()); return; }
    if (!logoTex) glGenTextures(1, &logoTex);
    glBindTexture(GL_TEXTURE_2D, logoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, logoW, logoH, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    imageFree(px);
}

void openVideo(const std::string& path) {
    std::string err;
    if (video.open(path, err)) {
        S.videoPath = path;
        S.videoOn = true;
        videoSerial = -1;
        toast(false, "Video layer: %s", video.name().c_str());
        saveSettings();
    } else {
        toast(true, "Video: %s", err.c_str());
    }
}

void drawOverlays(RenderTarget& rt, double t) {
    rt.bind();
    const float W = (float)rt.w, H = (float)rt.h;
    auto px2ndc = [&](float x, float y) { return ImVec2(x / W * 2 - 1, 1 - y / H * 2); };
    // video layer
    if (S.videoOn && S.layerSource == 2 && receiveSpout()) {
        rt.bind();
        float va = spoutRxW / (float)std::max(1, spoutRxH), ra = W / H, sx = 1, sy = 1;
        if (S.videoFit == 0) { if (va > ra) sx = va / ra; else sy = ra / va; }
        else if (S.videoFit == 1) { if (va > ra) sy = ra / va; else sx = va / ra; }
        gfx.layer(spoutRxTex, -sx, -sy, sx, sy, modOut.videoOpacity, true, S.videoBlend);
    }
    rt.bind();
    if (S.videoOn && S.layerSource != 2 && video.active()) {
        int w, h;
        bool bu;
        if (video.latest(videoSerial, videoBuf, w, h, bu)) {
            if (!videoTex) glGenTextures(1, &videoTex);
            glBindTexture(GL_TEXTURE_2D, videoTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, videoBuf.data());
            videoW = w;
            videoH = h;
            videoBottomUp = bu;
        }
        if (videoTex && videoW > 0) {
            float va = videoW / (float)videoH, ra = W / H, sx = 1, sy = 1;
            if (S.videoFit == 0) { if (va > ra) sx = va / ra; else sy = ra / va; }       // fill
            else if (S.videoFit == 1) { if (va > ra) sy = ra / va; else sx = va / ra; }  // fit
            gfx.layer(videoTex, -sx, -sy, sx, sy, modOut.videoOpacity, !videoBottomUp, S.videoBlend);
        }
    }
    // logo
    if (S.logoOn && !S.logoPath.empty()) {
        loadLogo();
        if (logoTex && logoW > 0) {
            float w = S.logoScale * W, h = w * logoH / (float)logoW, m = 0.03f * std::min(W, H);
            float x = m, y = m;
            if (S.logoPos == 1 || S.logoPos == 3) x = W - w - m;
            if (S.logoPos == 2 || S.logoPos == 3) y = H - h - m;
            if (S.logoPos == 4) { x = (W - w) / 2; y = (H - h) / 2; }
            ImVec2 a = px2ndc(x, y + h), b = px2ndc(x + w, y);
            gfx.layer(logoTex, a.x, a.y, b.x, b.y, modOut.logoOpacity, true, BlendNormal);
        }
    }
    // text
    bool djTitle = t < djTitleUntil && !djTitleText.empty();
    if (((S.textOn && !S.text.empty()) || djTitle) && overlayCtx) {
        ImGui::SetCurrentContext(overlayCtx);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(W, H);
        io.DeltaTime = 1 / 60.f;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (djTitle) {  // "Artist - Title" from the DJ link, fading in and out
            float a = (float)std::min({1.0, (t - (djTitleUntil - S.djTitleSecs)) * 2, (djTitleUntil - t) * 1.5});
            float sz = 0.06f * H;
            ImVec2 ts = fontOverlay->CalcTextSizeA(sz, FLT_MAX, 0, djTitleText.c_str());
            float x = (W - ts.x) / 2, y = H * 0.05f, o = std::max(2.f, sz * 0.04f);
            dl->AddText(fontOverlay, sz, ImVec2(x + o, y + o), IM_COL32(0, 0, 0, (int)(170 * a)), djTitleText.c_str());
            dl->AddText(fontOverlay, sz, ImVec2(x, y), IM_COL32(255, 255, 255, (int)(255 * a)), djTitleText.c_str());
        }
        if (S.textOn && !S.text.empty()) {
        float size = S.textSize * H;
        ImVec2 ts = fontOverlay->CalcTextSizeA(size, FLT_MAX, 0, S.text.c_str());
        float y = S.textPos == 0 ? H * 0.06f : S.textPos == 1 ? (H - ts.y) / 2 : H * 0.94f - ts.y;
        float x = (W - ts.x) / 2;
        if (S.textScroll > 0) x = W - (float)std::fmod(t * S.textScroll * W * 0.25, (double)(W + ts.x));
        ImU32 shadow = IM_COL32(0, 0, 0, (int)(160 * modOut.textOpacity));
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(S.textR, S.textG, S.textB, modOut.textOpacity));
        float o = std::max(2.f, size * 0.04f);
        dl->AddText(fontOverlay, size, ImVec2(x + o, y + o), shadow, S.text.c_str());
        dl->AddText(fontOverlay, size, ImVec2(x, y), col, S.text.c_str());
        }
        ImGui::Render();
        rt.bind();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGui::SetCurrentContext(uiCtx);
    }
}

// ===================================================================== render pipeline
void renderFrame(double t, double dt) {
    int w, h;
    SDL_GL_GetDrawableSize(outputs.empty() ? win : outputs[0].win, &w, &h);
    float sc = kQuality[S.quality].scale;
    w = std::max(16, (int)(w * sc)) & ~1;
    h = std::max(16, (int)(h * sc)) & ~1;
    if (w != renderW || h != renderH) {
        renderW = w;
        renderH = h;
    }
    applyModulation(beatNow());
    // Deck B is only rendered when it can be seen.
    bool needB = modOut.xfade > 0.001f || S.mixMode != MixCrossfade || S.showMixer || xanim.active;
    for (auto& o : outputs) needB |= o.source == 2;
    for (int d = 0; d < 2; d++) {
        if (d == 1 && !needB) continue;
        Deck& dk = decks[d];
        if (!frozen) {
            dk.time += dt * S.speed;
            dk.visible().requestFrame(dk.time, w, h);
        }
        // the spare renders while it is loading (to warm up) or fading out
        if (dk.spare().loading() || dk.blending) dk.spare().requestFrame(dk.time, w, h);
        unsigned cur = dk.visible().texture();
        if (dk.blending) {
            float k = (float)std::clamp((t - dk.blendStart) / std::max(0.01, dk.blendDur), 0.0, 1.0);
            dk.rt.ensure(w, h);
            gfx.mix(dk.rt, dk.spare().texture(), cur, k * k * (3 - 2 * k), MixCrossfade);
            dk.outTex = dk.rt.tex;
            if (k >= 1) dk.blending = false;
        } else {
            dk.outTex = cur;
        }
    }
    mixRT.ensure(w, h);
    for (auto& r : fxRT) r.ensure(w, h);
    finalRT.ensure(w, h);
    if (xanim.active && S.transition > 0 && needB) {
        bool toB = xanim.to > xanim.from;
        float p = toB ? S.xfade : 1 - S.xfade;
        gfx.transition(mixRT, decks[toB ? 0 : 1].outTex, decks[toB ? 1 : 0].outTex, std::clamp(p, 0.f, 1.f), S.transition, (float)t);
    } else {
        gfx.mix(mixRT, decks[0].outTex, needB ? decks[1].outTex : decks[0].outTex, needB ? modOut.xfade : 0.f, S.mixMode);
    }
    fxRotation += (float)(S.fx.rotateSpeed * dt);
    int prev = fxIdx;
    fxIdx = 1 - fxIdx;
    gfx.fx(fxRT[fxIdx], mixRT.tex, fxRT[prev].tex, modOut.fx, fxRotation + modOut.rotOffset, (float)t);
    drawOverlays(fxRT[fxIdx], t);
    // strobe: on-beat or free-running white flashes (short duty cycle)
    float strobe = 0;
    if (modOut.fx.strobe) {
        double ph = S.fx.strobeOnBeat ? std::fmod(beatNow() * std::max(1.f, S.fx.strobeRate / 4.f), 1.0) : std::fmod(t * S.fx.strobeRate, 1.0);
        strobe = ph < 0.25 ? 1.f : 0.f;
    }
    gfx.finalize(finalRT, fxRT[fxIdx].tex, blackAmt, std::max(flashAmt, strobe));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (S.spout) {
        if (!spoutNamed) { spout.SetSenderName("Amethyst"); spoutNamed = true; }
        spout.SendTexture(finalRT.tex, GL_TEXTURE_2D, finalRT.w, finalRT.h, true, 0);
    }
    captureForRecording(dt);
    sendDmx(t, strobe);
}

// ===================================================================== actions
void toggleAllUi();
void openEditor();
void dialogAddFiles();
void dialogAddFolder();
float logNorm(float v, float lo, float hi) { return (float)(std::log(v / lo) / std::log(hi / lo)); }
float logDenorm(float n, float lo, float hi) { return lo * std::pow(hi / lo, n); }

void registerActions() {
    auto trig = [](const char* id, const char* label, const char* group, std::function<void()> f) {
        Action a;
        a.id = id; a.label = label; a.group = group; a.kind = Action::Trigger; a.trigger = std::move(f);
        actions.add(a);
    };
    auto toggle = [](const char* id, const char* label, const char* group, bool* flag, std::function<void()> after = nullptr) {
        Action a;
        a.id = id; a.label = label; a.group = group; a.kind = Action::Toggle;
        a.trigger = [flag, after, label] { *flag = !*flag; if (after) after(); toast(false, "%s %s", label, *flag ? "on" : "off"); };
        a.get = [flag] { return *flag ? 1.f : 0.f; };
        actions.add(a);
    };
    auto cont = [](const char* id, const char* label, const char* group, std::function<void(float)> set, std::function<float()> get) {
        Action a;
        a.id = id; a.label = label; a.group = group; a.kind = Action::Continuous; a.set = std::move(set); a.get = std::move(get);
        actions.add(a);
    };

    trig("next", "Next", "Presets", [] { requestLaunch(focus, -1, 1, false, false, false); });
    trig("prev", "Previous", "Presets", [] { back(focus, false); });
    trig("random", "Random", "Presets", [] { requestLaunch(focus, -1, 1, false, true, false); });
    trig("next_hard", "Next (cut)", "Presets", [] { go(focus, 1, true); });
    trig("random_hard", "Random (cut)", "Presets", [] { go(focus, 1, true, true); });
    toggle("lock", "Lock", "Presets", &locked);
    trig("favorite", "Favorite", "Presets", [] { toggleFavorite(decks[focus].cur); });
    for (int i = 0; i < 10; i++) {
        static char ids[10][8], labels[10][8];
        snprintf(ids[i], 8, "cue%d", (i + 1) % 10);
        snprintf(labels[i], 8, "Cue %d", (i + 1) % 10);
        trig(ids[i], labels[i], "Cues", [i] { fireCue(i); });
    }

    trig("focus_toggle", "Focus A/B", "Mixer", [] { focus = 1 - focus; toast(false, "Focus: deck %s", decks[focus].name); });
    trig("a_next", "A next", "Mixer", [] { go(0, 1, false); });
    trig("a_random", "A random", "Mixer", [] { go(0, 1, false, true); });
    trig("b_next", "B next", "Mixer", [] { go(1, 1, false); });
    trig("b_random", "B random", "Mixer", [] { go(1, 1, false, true); });
    cont("crossfade", "Crossfader", "Mixer", [](float v) { S.xfade = v; xanim.active = false; }, [] { return S.xfade; });
    trig("xfade_auto", "Auto crossfade", "Mixer", [] { startXfade(S.xfade < 0.5f ? 1.f : 0.f, transitionSeconds()); });
    trig("mix_mode", "Mix mode", "Mixer", [] { S.mixMode = (S.mixMode + 1) % MixModeCount; toast(false, "Mix: %s", mixModeName(S.mixMode)); });
    toggle("auto_decks", "Auto-mix decks", "Mixer", &S.autoDecks);
    toggle("smart_mix", "Smart auto-mix (follow the song)", "Auto-pilot", &S.smartMix);

    toggle("auto", "Auto-pilot", "Auto-pilot", &S.autoOn, [] { decks[0].lastSwitch = decks[1].lastSwitch = now(); });
    toggle("shuffle", "Shuffle", "Auto-pilot", &S.shuffle);
    trig("quantize", "Quantize", "Auto-pilot", [] { S.quantize = (S.quantize + 1) % 4; toast(false, "Launch quantize: %s", kQuantizeNames[S.quantize]); });
    cont("interval", "Interval", "Auto-pilot", [](float v) { S.interval = (int)logDenorm(v, 4, 300); }, [] { return logNorm((float)S.interval, 4, 300); });
    trig("tap", "Tap tempo", "Tempo", [] { tempo.tap(now()); });
    trig("tempo_auto", "Auto BPM", "Tempo", [] { tempo.setSource(Tempo::Source::Auto); toast(false, "Tempo: automatic detection"); });
    trig("link", "Ableton Link", "Tempo", [] {
        S.link = !tempo.linkEnabled();
        tempo.enableLink(S.link);
        if (S.link) tempo.setSource(Tempo::Source::Link);
        toast(false, S.link ? "Ableton Link on" : "Ableton Link off");
    });

    toggle("blackout", "Blackout", "Performance", &blackoutOn);
    trig("flash", "Flash", "Performance", [] { flashAmt = 1; });
    { Action a; a.id = "freeze"; a.label = "Freeze (hold)"; a.group = "Performance"; a.kind = Action::Momentary;
      a.set = [](float v) { frozen = v > 0.5f; }; a.get = [] { return frozen ? 1.f : 0.f; }; actions.add(a); }
    cont("speed", "Speed", "Performance", [](float v) { S.speed = logDenorm(v, 0.1f, 4.f); }, [] { return logNorm(S.speed, 0.1f, 4.f); });
    trig("speed_reset", "Speed 1x", "Performance", [] { S.speed = 1; });
    cont("gain", "Sensitivity", "Performance", [](float v) { S.gain = logDenorm(v, 0.05f, 8.f); }, [] { return logNorm(std::clamp(S.gain, 0.05f, 8.f), 0.05f, 8.f); });
    cont("blend", "Blend time", "Performance", [](float v) { S.blend = v * 12; }, [] { return S.blend / 12; });

    toggle("fx_kaleido", "Kaleidoscope", "FX", &S.fx.kaleido);
    toggle("fx_mirror", "Mirror", "FX", &S.fx.mirror);
    toggle("fx_feedback", "Feedback", "FX", &S.fx.feedback);
    toggle("fx_rgb", "RGB split", "FX", &S.fx.rgbSplit);
    toggle("fx_pixel", "Pixelate", "FX", &S.fx.pixelate);
    toggle("fx_invert", "Invert", "FX", &S.fx.invert);
    toggle("fx_strobe", "Strobe", "FX", &S.fx.strobe);
    cont("fx_hue", "Hue", "FX", [](float v) { S.fx.hue = v; }, [] { return S.fx.hue; });
    cont("fx_zoom", "Zoom", "FX", [](float v) { S.fx.zoom = logDenorm(v, 0.4f, 4.f); }, [] { return logNorm(S.fx.zoom, 0.4f, 4.f); });
    cont("fx_rotate", "Rotate", "FX", [](float v) { S.fx.rotateSpeed = (v - 0.5f) * 4; }, [] { return S.fx.rotateSpeed / 4 + 0.5f; });
    cont("fx_swirl", "Swirl", "FX", [](float v) { S.fx.swirl = (v - 0.5f) * 4; }, [] { return S.fx.swirl / 4 + 0.5f; });
    cont("fx_feedback_amt", "Trails", "FX", [](float v) { S.fx.feedbackAmount = v * 0.98f; }, [] { return S.fx.feedbackAmount / 0.98f; });
    trig("fx_reset", "Reset FX", "FX", [] { S.fx = FxParams{}; toast(false, "Effects reset"); });

    toggle("logo", "Logo", "Overlays", &S.logoOn);
    toggle("text", "Text", "Overlays", &S.textOn);
    toggle("video", "Video layer", "Overlays", &S.videoOn, [] { if (S.videoOn && !video.active() && !S.videoPath.empty()) openVideo(S.videoPath); });
    cont("video_opacity", "Video opacity", "Overlays", [](float v) { S.videoOpacity = v; }, [] { return S.videoOpacity; });

    trig("record", "Record", "Output", [] { recorder.active() ? stopRecording() : startRecording(); });
    trig("spout", "Spout", "Output", [] { setSpout(!S.spout); });
    trig("output", "Output window", "Output", [] { if (!outputs.empty()) closeOutput(0); else openOutput(-2); });

    // ---- actions that used to be hard-wired keys; now freely re-assignable
    auto step = [&](const char* id, const char* label, const char* group, std::function<void()> f) {
        trig(id, label, group, std::move(f));
        actions.all().back().repeat = true;
    };
    auto local = [&](const char* id, const char* label, const char* group, std::function<void()> f) {
        trig(id, label, group, std::move(f));
        actions.all().back().remote = false;
    };
    trig("prev_hard", "Previous (cut)", "Presets", [] { back(focus, true); });
    step("blend_down", "Blend time -", "Presets", [] { S.blend = stepIn(kBlendSteps, S.blend, -1); toast(false, "Blend %.2gs", S.blend); });
    step("blend_up", "Blend time +", "Presets", [] { S.blend = stepIn(kBlendSteps, S.blend, 1); toast(false, "Blend %.2gs", S.blend); });
    for (int i = 0; i < 10; i++) {
        static char sid[10][16], slab[10][24], cid[10][16], clab[10][24];
        snprintf(sid[i], 16, "store_cue%d", (i + 1) % 10);
        snprintf(slab[i], 24, "Store cue %d", (i + 1) % 10);
        snprintf(cid[i], 16, "clear_cue%d", (i + 1) % 10);
        snprintf(clab[i], 24, "Clear cue %d", (i + 1) % 10);
        local(sid[i], slab[i], "Cues", [i] { storeCue(i); });
        local(cid[i], clab[i], "Cues", [i] { clearCue(i); });
    }
    step("xfade_left", "Crossfader to A", "Mixer", [] { S.xfade = std::max(0.f, S.xfade - 0.05f); xanim.active = false; });
    step("xfade_right", "Crossfader to B", "Mixer", [] { S.xfade = std::min(1.f, S.xfade + 0.05f); xanim.active = false; });
    step("interval_down", "Interval -", "Auto-pilot", [] { S.interval = stepIn(kIntervalSteps, S.interval, -1); toast(false, "Auto interval %ds", S.interval); });
    step("interval_up", "Interval +", "Auto-pilot", [] { S.interval = stepIn(kIntervalSteps, S.interval, 1); toast(false, "Auto interval %ds", S.interval); });
    trig("tempo_source", "Tempo source", "Tempo", [] {
        int s = ((int)tempo.source() + 1) % 3;
        if (s == 2 && !tempo.linkEnabled()) { tempo.enableLink(true); S.link = true; }
        tempo.setSource((Tempo::Source)s);
        toast(false, "Tempo source: %s", tempo.sourceName());
    });
    step("speed_down", "Speed -", "Performance", [] { S.speed = stepIn(kSpeedSteps, S.speed, -1); toast(false, "Speed %.2gx", S.speed); });
    step("speed_up", "Speed +", "Performance", [] { S.speed = stepIn(kSpeedSteps, S.speed, 1); toast(false, "Speed %.2gx", S.speed); });
    step("gain_up", "Sensitivity +", "Performance", [] { setGain(S.gain * 1.2f); });
    step("gain_down", "Sensitivity -", "Performance", [] { setGain(S.gain / 1.2f); });
    trig("quality", "Render quality", "Performance", [] {
        S.quality = (S.quality + 1) % 4;
        for (auto& d : decks) for (auto& p : d.players) p.setMesh(kQuality[S.quality].meshW, kQuality[S.quality].meshH);
        toast(false, "Quality: %s", kQuality[S.quality].label);
    });
    for (int r = 0; r <= 5; r++) {
        static char rid[6][8], rlab[6][24];
        snprintf(rid[r], 8, "rate%d", r);
        snprintf(rlab[r], 24, r ? "Rate %d stars" : "Clear rating", r);
        local(rid[r], rlab[r], "Library", [r] { setRating(decks[focus].cur, r); });
    }
    local("add_setlist", "Add to active setlist", "Library", [] { addToSetlist(decks[focus].cur, S.activeSetlist.empty() ? "Setlist 1" : S.activeSetlist); });
    local("add_files", "Add preset files", "Library", [] { dialogAddFiles(); });
    local("add_folder", "Add preset folder", "Library", [] { dialogAddFolder(); });
    local("rescan", "Rescan library", "Library", [] { rescan(); toast(false, "Library rescanned — %d presets", (int)lib.size()); });
    local("folder_filter", "Next folder filter", "Library", [] {
        sourceFilter = sourceFilter + 1 >= (int)sourceNames.size() ? -1 : sourceFilter + 1;
        rebuildView();
        toast(false, "Folder: %s (%d)", sourceFilter < 0 ? "All" : sourceNames[sourceFilter].c_str(), (int)view.size());
    });
    local("search", "Search presets", "Library", [] { S.showBrowser = true; S.uiHidden = false; focusSearch = true; });
    local("editor", "Preset editor", "Library", [] { if (editorOpen) editorOpen = false; else openEditor(); });

    auto panel = [&](const char* id, const char* label, bool* flag) {
        local(id, label, "View", [flag] { *flag = !*flag || S.uiHidden; S.uiHidden = false; });
    };
    local("ui_all", "Hide / show all interface", "View", [] { toggleAllUi(); });
    panel("panel_browser", "Preset browser", &S.showBrowser);
    panel("panel_hud", "Now-playing bar", &S.showHud);
    panel("panel_cues", "Cue strip", &S.showCues);
    panel("panel_mixer", "Mixer panel", &S.showMixer);
    panel("panel_fx", "Effects panel", &S.showFx);
    panel("panel_overlays", "Overlays panel", &S.showOverlays);
    panel("panel_outputs", "Outputs panel", &S.showOutputs);
    panel("panel_control", "Control panel", &S.showControl);
    panel("panel_setlists", "Setlists panel", &S.showSetlists);
    local("panel_menu", "Menu bar", "View", [] { S.showMenu = !S.showMenu; S.uiHidden = false; });
    local("previews", "Live previews", "View", [] { S.livePreviews = !S.livePreviews; toast(false, S.livePreviews ? "Live previews on" : "Live previews off"); });
    local("fullscreen", "Fullscreen", "View", [] { toggleFullscreen(keyWindow ? keyWindow : win); });
    local("help", "Shortcut help", "View", [] { helpOpen = !helpOpen; if (helpOpen) S.uiHidden = false; });
    local("keyboard", "Keyboard layout editor", "View", [] { keyboardOpen = !keyboardOpen; if (keyboardOpen) S.uiHidden = false; });
    local("audio_menu", "Audio input menu", "Audio", [] { refreshDevices(); audioMenuOpen = true; audioMenuSel = 0; S.uiHidden = false; });
    local("audio_file", "Play audio file", "Audio", [] { playAudioFileDialog(); });
    local("audio_pause", "Pause / resume file", "Audio", [] {
        if (audio.source() != AudioEngine::Source::File) toast(false, "No audio file playing");
        else { audio.togglePause(); toast(false, audio.paused() ? "Paused" : "Playing"); }
    });
    local("quit", "Quit", "View", [] { running = false; });
    for (int i = 0; i < kScenes; i++) {
        static char id[16][16], lb[16][24], sid[16][24], slb[16][24];
        snprintf(id[i], 16, "scene%d", i + 1);
        snprintf(lb[i], 24, "Scene %d", i + 1);
        snprintf(sid[i], 24, "store_scene%d", i + 1);
        snprintf(slb[i], 24, "Store scene %d", i + 1);
        trig(id[i], lb[i], "Scenes", [i] { recallScene(i); });
        local(sid[i], slb[i], "Scenes", [i] { storeScene(i); });
    }
    trig("timeline_play", "Play / stop show", "Scenes", [] { timelinePlay(!tlPlaying); });
    actions.all().back().get = [] { return tlPlaying ? 1.f : 0.f; };
    trig("timeline_next", "Next show step", "Scenes", [] {
        if (timeline.empty()) return;
        tlIndex = (tlIndex + 1) % (int)timeline.size();
        tlStepStart = std::floor(beatNow() / 4) * 4;
        recallScene(timeline[tlIndex].scene);
    });
    trig("transition", "Transition type", "Mixer", [] { S.transition = (S.transition + 1) % TransitionCount; toast(false, "Transition: %s", transitionName(S.transition)); });
    trig("dj_link", "DJ link", "Tempo", [] { setDjLink(!S.djOn); });
    actions.all().back().get = [] { return S.djOn ? 1.f : 0.f; };
    trig("dmx", "Art-Net lighting", "Output", [] { S.dmxOn = !S.dmxOn; toast(false, S.dmxOn ? "Art-Net on" : "Art-Net off"); });
    actions.all().back().get = [] { return S.dmxOn ? 1.f : 0.f; };
    panel("panel_scenes", "Scenes panel", &S.showScenes);
    panel("panel_mod", "Modulation panel", &S.showMod);
    local("similar", "More like the current preset", "Library", [] { if (decks[focus].cur >= 0) setPool("similar:" + lib[decks[focus].cur].path); });
}

// ===================================================================== key map
// keymap: (key, modifier bits: 1 Shift, 2 Ctrl, 4 Alt) -> action id. Fully user-editable.
void defaultKeymap() {
    keymap.clear();
    auto b = [](SDL_Keycode k, int m, const char* id) { keymap[{(int)k, m}] = id; };
    b(SDLK_SPACE, 0, "next"); b(SDLK_RIGHT, 0, "next"); b(SDLK_SPACE, 1, "next_hard"); b(SDLK_RIGHT, 1, "next_hard");
    b(SDLK_LEFT, 0, "prev"); b(SDLK_LEFT, 1, "prev_hard"); b(SDLK_r, 0, "random"); b(SDLK_r, 1, "random_hard");
    b(SDLK_LEFTBRACKET, 0, "blend_down"); b(SDLK_RIGHTBRACKET, 0, "blend_up"); b(SDLK_l, 0, "lock");
    for (int i = 0; i < 10; i++) {
        SDL_Keycode k = i == 9 ? SDLK_0 : SDLK_1 + i;
        std::string n = std::to_string((i + 1) % 10);
        keymap[{(int)k, 0}] = "cue" + n;
        keymap[{(int)k, 1}] = "store_cue" + n;
        keymap[{(int)k, 4}] = "clear_cue" + n;
        if (i < 5) keymap[{(int)k, 2}] = "rate" + std::to_string(i + 1);
    }
    b(SDLK_0, 2, "rate0");
    b(SDLK_TAB, 0, "focus_toggle"); b(SDLK_BACKSLASH, 0, "xfade_auto"); b(SDLK_SPACE, 2, "xfade_auto");
    b(SDLK_LEFT, 2, "xfade_left"); b(SDLK_RIGHT, 2, "xfade_right"); b(SDLK_j, 0, "mix_mode");
    b(SDLK_a, 0, "auto"); b(SDLK_COMMA, 0, "interval_down"); b(SDLK_PERIOD, 0, "interval_up"); b(SDLK_k, 0, "quantize"); b(SDLK_s, 0, "shuffle");
    b(SDLK_t, 0, "tap"); b(SDLK_t, 1, "tempo_source");
    b(SDLK_b, 0, "blackout"); b(SDLK_x, 0, "flash"); b(SDLK_z, 0, "freeze");
    b(SDLK_MINUS, 0, "speed_down"); b(SDLK_EQUALS, 0, "speed_up"); b(SDLK_BACKSPACE, 0, "speed_reset");
    b(SDLK_UP, 0, "gain_up"); b(SDLK_DOWN, 0, "gain_down"); b(SDLK_q, 0, "quality");
    b(SDLK_F2, 0, "fx_kaleido"); b(SDLK_F3, 0, "fx_mirror"); b(SDLK_F4, 0, "fx_feedback"); b(SDLK_F5, 0, "fx_rgb");
    b(SDLK_F6, 0, "fx_pixel"); b(SDLK_F7, 0, "fx_invert"); b(SDLK_F8, 0, "fx_strobe"); b(SDLK_F9, 0, "record"); b(SDLK_F12, 0, "spout");
    b(SDLK_f, 0, "fullscreen"); b(SDLK_F11, 0, "fullscreen"); b(SDLK_o, 0, "output"); b(SDLK_u, 0, "ui_all");
    b(SDLK_p, 0, "panel_browser"); b(SDLK_n, 0, "panel_hud"); b(SDLK_c, 0, "panel_cues"); b(SDLK_d, 0, "panel_mixer");
    b(SDLK_w, 0, "panel_fx"); b(SDLK_e, 0, "editor"); b(SDLK_F10, 0, "panel_menu"); b(SDLK_v, 0, "previews");
    b(SDLK_SLASH, 0, "search"); b(SDLK_f, 2, "search"); b(SDLK_SLASH, 1, "help"); b(SDLK_h, 0, "help"); b(SDLK_F1, 0, "help");
    b(SDLK_k, 2, "keyboard"); b(SDLK_g, 0, "folder_filter"); b(SDLK_i, 0, "audio_menu"); b(SDLK_m, 0, "audio_file"); b(SDLK_m, 1, "audio_pause");
    b(SDLK_o, 2, "add_files"); b(SDLK_o, 3, "add_folder"); b(SDLK_r, 2, "rescan"); b(SDLK_q, 2, "quit");
    b(SDLK_d, 2, "favorite"); b(SDLK_l, 2, "add_setlist");
    for (int i = 0; i < 8; i++) {
        keymap[{(int)(SDLK_F1 + i), 1}] = "scene" + std::to_string(i + 1);
        keymap[{(int)(SDLK_F1 + i), 3}] = "store_scene" + std::to_string(i + 1);
    }
    b(SDLK_t, 2, "timeline_play"); b(SDLK_j, 1, "transition"); b(SDLK_y, 0, "panel_mod"); b(SDLK_F1, 2, "panel_scenes"); b(SDLK_m, 2, "similar");
}

// Keypad keys act like their main-keyboard twins.
SDL_Keycode normalizeKey(SDL_Keycode k) {
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return SDLK_1 + (k - SDLK_KP_1);
    switch (k) {
        case SDLK_KP_0: return SDLK_0;
        case SDLK_KP_ENTER: return SDLK_RETURN;
        case SDLK_KP_PLUS: return SDLK_EQUALS;
        case SDLK_KP_MINUS: return SDLK_MINUS;
        case SDLK_KP_DIVIDE: return SDLK_SLASH;
        case SDLK_KP_PERIOD: return SDLK_PERIOD;
        default: return k;
    }
}

std::string keyName(SDL_Keycode k) {
    switch (k) {
        case SDLK_SPACE: return "Space";
        case SDLK_RETURN: return "Enter";
        case SDLK_BACKSPACE: return "Backspace";
        case SDLK_LEFT: return "Left";
        case SDLK_RIGHT: return "Right";
        case SDLK_UP: return "Up";
        case SDLK_DOWN: return "Down";
        case SDLK_PAGEUP: return "PgUp";
        case SDLK_PAGEDOWN: return "PgDn";
        case SDLK_DELETE: return "Del";
        case SDLK_INSERT: return "Ins";
        default: break;
    }
    const char* n = SDL_GetKeyName(k);
    return n && *n ? n : "?";
}

std::string chordName(int key, int mods) {
    std::string s;
    if (mods & 2) s += "Ctrl+";
    if (mods & 4) s += "Alt+";
    if (mods & 1) s += "Shift+";
    return s + keyName(key);
}

std::vector<std::string> chordsFor(const std::string& id) {
    std::vector<std::string> out;
    for (auto& [k, a] : keymap)
        if (a == id) out.push_back(chordName(k.first, k.second));
    return out;
}

// Short "[key]" hint for UI labels, reflecting the current key map.
std::string keyHint(const std::string& id) {
    auto c = chordsFor(id);
    return c.empty() ? "" : " [" + c.front() + "]";
}

void bindKey(int key, int mods, const std::string& id) {
    if (id.empty()) keymap.erase({key, mods});
    else keymap[{key, mods}] = id;
    saveSettings();
    Action* a = actions.find(id);
    if (!id.empty()) toast(false, "%s  ->  %s", chordName(key, mods).c_str(), a ? a->label.c_str() : id.c_str());
}

// ===================================================================== MIDI / OSC / web
void processMidi() {
    for (auto& m : midi.poll()) {
        int type = m.status & 0xF0, ch = m.status & 0x0F;
        bool isNote = type == 0x90 || type == 0x80, isCC = type == 0xB0;
        if (!isNote && !isCC) continue;
        std::string dev = midi.deviceName(m.device);
        if (!midi.learning.empty() && (isCC || (type == 0x90 && m.d2 > 0))) {
            int t = isCC ? 1 : 0;
            midi.bindings.erase(std::remove_if(midi.bindings.begin(), midi.bindings.end(),
                                               [&](const MidiBinding& b) { return b.device == dev && b.type == t && b.channel == ch && b.number == m.d1; }),
                                midi.bindings.end());
            midi.bindings.push_back({midi.learning, dev, t, ch, m.d1});
            Action* a = actions.find(midi.learning);
            toast(false, "MIDI: %s -> %s", Midi::describe(midi.bindings.back()).c_str(), a ? a->label.c_str() : "?");
            midi.learning.clear();
            saveSettings();
            continue;
        }
        for (auto& b : midi.bindings) {
            if (!b.device.empty() && b.device != dev) continue;
            if (b.channel != ch || b.number != m.d1 || b.type != (isCC ? 1 : 0)) continue;
            Action* a = actions.find(b.action);
            if (!a) continue;
            if (isNote) {
                bool down = type == 0x90 && m.d2 > 0;
                actions.invoke(b.action, down, a->kind == Action::Continuous && down, m.d2 / 127.f);
            } else if (a->kind == Action::Continuous) {
                actions.invoke(b.action, true, true, m.d2 / 127.f);
            } else {
                bool high = m.d2 >= 64;
                if (high != b.lastHigh) actions.invoke(b.action, high);
                b.lastHigh = high;
            }
        }
    }
}

void handleRemote(const std::string& rawAddr, bool hasValue, float value) {
    std::string a = rawAddr;
    if (a.rfind("/amethyst/", 0) == 0) a = a.substr(10);
    else if (!a.empty() && a[0] == '/') a = a.substr(1);
    for (auto& c : a)
        if (c == '/') c = '_';
    if (a.rfind("cue_", 0) == 0) a = "cue" + a.substr(4);
    Action* act = actions.find(a);
    if (!act) return;
    if (act->kind == Action::Continuous) actions.invoke(a, true, hasValue, value);
    else if (act->kind == Action::Momentary) actions.invoke(a, hasValue ? value > 0.5f : true);
    else if (!hasValue || value > 0.5f) actions.invoke(a, true);
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}

HttpResponse handleHttp(const std::string&, const std::string& path, const std::string& query) {
    HttpResponse r;
    if (path == "/" || path == "/index.html") {
        r.contentType = "text/html; charset=utf-8";
        r.body = kRemotePage;
    } else if (path == "/actions") {
        r.contentType = "application/json";
        r.body = "[";
        bool first = true;
        for (auto& a : actions.all()) {
            if (!a.remote) continue;
            const char* kind = a.kind == Action::Continuous ? "continuous" : a.kind == Action::Toggle ? "toggle" : a.kind == Action::Momentary ? "momentary" : "trigger";
            r.body += std::string(first ? "" : ",") + "{\"id\":\"" + a.id + "\",\"label\":\"" + jsonEscape(a.label) + "\",\"group\":\"" + a.group + "\",\"kind\":\"" + kind + "\"}";
            first = false;
        }
        r.body += "]";
    } else if (path == "/state") {
        r.contentType = "application/json";
        char b[256];
        double beat = beatNow();
        snprintf(b, sizeof b, "{\"x\":%.3f,\"bpm\":%.2f,\"src\":\"%s\",\"beat\":%d,", S.xfade, tempo.bpm(), tempo.sourceName(), ((int)std::floor(beat) % 4 + 4) % 4);
        r.body = b;
        r.body += "\"a\":\"" + jsonEscape(decks[0].cur >= 0 ? lib[decks[0].cur].name : "-") + "\",\"b\":\"" + jsonEscape(decks[1].cur >= 0 ? lib[decks[1].cur].name : "-") + "\",\"values\":{";
        bool first = true;
        for (auto& a : actions.all())
            if (a.get) {
                snprintf(b, sizeof b, "%s\"%s\":%.3f", first ? "" : ",", a.id.c_str(), a.get());
                r.body += b;
                first = false;
            }
        r.body += "}}";
    } else if (path.rfind("/a/", 0) == 0) {
        std::string id = path.substr(3);
        bool has = false;
        float v = 1;
        auto p = query.find("v=");
        if (p != std::string::npos) { has = true; v = (float)std::atof(query.c_str() + p + 2); }
        handleRemote("/" + id, has, v);
        r.body = "ok";
    } else {
        r.status = 404;
        r.body = "not found";
    }
    return r;
}

void applyNetSettings() {
    std::string err;
    if (S.oscOn && (!osc.running() || osc.port() != S.oscPort)) {
        if (!osc.start(S.oscPort, err)) { toast(true, "OSC: %s", err.c_str()); S.oscOn = false; }
    } else if (!S.oscOn && osc.running()) osc.stop();
    if (S.webOn && (!web.running() || web.port() != S.webPort)) {
        if (!web.start(S.webPort, err)) { toast(true, "Web remote: %s", err.c_str()); S.webOn = false; }
    } else if (!S.webOn && web.running()) web.stop();
}

// ===================================================================== files & drops
void addPresetFiles(const std::vector<std::string>& files) {
    std::string firstPath;
    for (auto& f : files) {
        if (std::find(S.files.begin(), S.files.end(), f) == S.files.end()) S.files.push_back(f);
        if (firstPath.empty()) firstPath = f;
    }
    rescan();
    saveSettings();
    toast(false, "Added %d preset%s", (int)files.size(), files.size() == 1 ? "" : "s");
    auto it = byPath.find(firstPath);
    if (it != byPath.end()) loadPreset(focus, it->second, false);
}
void dialogAddFiles() {
    auto files = plat::pickFiles(hwndOf(win), L"Add MilkDrop presets", {{L"MilkDrop presets", L"*.milk;*.prjm"}}, true);
    if (!files.empty()) addPresetFiles(files);
}
void dialogAddFolder() {
    auto d = plat::pickFolder(hwndOf(win), L"Add a folder of MilkDrop presets");
    if (!d.empty()) addFolder(d);
}

std::vector<std::string> pendingDrops;
void processDrops() {
    if (pendingDrops.empty()) return;
    std::vector<std::string> presets;
    for (auto& d : pendingDrops) {
        fs::path p = fs::u8path(d);
        std::error_code ec;
        if (fs::is_directory(p, ec)) addFolder(d);
        else if (isPresetFile(p)) presets.push_back(d);
        else if (isAudioFile(p)) startAudioFrom("file", d, false);
        else if (isVideoFile(p)) openVideo(d);
        else if (isImageFile(p)) { S.logoPath = d; S.logoOn = true; toast(false, "Logo overlay: %s", p.filename().u8string().c_str()); saveSettings(); }
        else toast(true, "Can't use %s", p.filename().u8string().c_str());
    }
    if (!presets.empty()) addPresetFiles(presets);
    pendingDrops.clear();
}

// ===================================================================== preset editor
void openEditor() {
    int cur = decks[focus].cur;
    editorOpen = true;
    S.uiHidden = false;
    if (cur < 0) return;
    std::ifstream f(fs::u8path(lib[cur].path), std::ios::binary);
    editorText.assign((std::istreambuf_iterator<char>(f)), {});
    editorPath = lib[cur].path;
    editorDirty = false;
    editorApplyAt = 0;
    visualEditor.open(editorText);
    editorStatus = "Editing deck " + std::string(decks[focus].name) + ": " + lib[cur].name;
}
void applyEditor() {
    Deck& d = decks[focus];
    d.blending = false;
    d.pendingIdx = d.cur;
    d.pendingToken = ++loadTokens;
    d.pendingEditor = true;
    d.spare().loadData(editorText, d.pendingToken);
    editorStatus = "Compiling...";
}
void saveEditorAs() {
    std::string def = editorPath.empty() ? "My preset.milk" : fs::u8path(editorPath).stem().u8string() + " (edit).milk";
    auto out = plat::pickSaveFile(hwndOf(win), L"Save preset as", {{L"MilkDrop preset", L"*.milk"}}, def, L"milk");
    if (out.empty()) return;
    std::ofstream f(fs::u8path(out), std::ios::binary | std::ios::trunc);
    f << editorText;
    f.close();
    editorPath = out;
    editorDirty = false;
    editorStatus = "Saved " + out;
    addPresetFiles({out});
}

// ===================================================================== keyboard
void toggleAllUi() {
    S.uiHidden = !S.uiHidden;
    if (!S.uiHidden) toast(false, "Interface shown");
}

int digitSlot(SDL_Keycode k, SDL_Scancode sc) {
    if (k >= SDLK_1 && k <= SDLK_9) return k - SDLK_1;
    if (k == SDLK_0) return 9;
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return k - SDLK_KP_1;
    if (k == SDLK_KP_0) return 9;
    bool printable = k >= 32 && k < 127, letter = k >= 'a' && k <= 'z';
    if (printable && !letter && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) return sc - SDL_SCANCODE_1;
    return -1;
}

void audioMenuActivate(int i);

bool handleKey(const SDL_KeyboardEvent& e, bool textInputActive) {
    const SDL_Keycode k = e.keysym.sym;
    const Uint16 mod = e.keysym.mod;
    const bool sh = mod & KMOD_SHIFT, ctrl = mod & KMOD_CTRL, alt = mod & KMOD_ALT, rep = e.repeat;
    SDL_Window* from = SDL_GetWindowFromID(e.windowID);

    if (textInputActive) {
        if (k == SDLK_ESCAPE) { defocusSearch = true; return true; }  // leave the field, keep its text
        if (!searchHasFocus) {  // editor or another text field
            if (editorOpen && ctrl && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) { applyEditor(); return true; }
            if (editorOpen && ctrl && k == SDLK_s) { saveEditorAs(); return true; }
            return false;
        }
        switch (k) {
            case SDLK_DOWN: sel = std::min((int)view.size() - 1, sel + 1); scrollToSel = true; return true;
            case SDLK_UP: sel = std::max(0, sel - 1); scrollToSel = true; return true;
            case SDLK_PAGEDOWN: sel = std::min((int)view.size() - 1, sel + 10); scrollToSel = true; return true;
            case SDLK_PAGEUP: sel = std::max(0, sel - 10); scrollToSel = true; return true;
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (sel >= 0 && sel < (int)view.size()) loadPreset(focus, view[sel], sh);
                return true;
            default: return false;
        }
    }
    if (k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL || k == SDLK_RCTRL || k == SDLK_LALT || k == SDLK_RALT || k == SDLK_LGUI || k == SDLK_RGUI)
        return false;
    if (audioMenuOpen) {
        int count = (int)audioDevices.size() + 2;
        if (k == SDLK_DOWN) audioMenuSel = (audioMenuSel + 1) % count;
        else if (k == SDLK_UP) audioMenuSel = (audioMenuSel + count - 1) % count;
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) audioMenuActivate(audioMenuSel);
        else if (k == SDLK_ESCAPE || k == SDLK_i) audioMenuOpen = false;
        return true;
    }
    if (!midi.learning.empty() && k == SDLK_ESCAPE) { midi.learning.clear(); toast(false, "MIDI learn cancelled"); return true; }
    if (k == SDLK_ESCAPE) {
        if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return false;
        if (!captureAction.empty()) { captureAction.clear(); toast(false, "Key capture cancelled"); }
        else if (helpOpen) helpOpen = false;
        else if (keyboardOpen) keyboardOpen = false;
        else if (editorOpen) editorOpen = false;
        else if (!query.empty()) { query.clear(); searchBuf[0] = 0; rebuildView(); toast(false, "Search cleared"); }
        else if (from && (SDL_GetWindowFlags(from) & SDL_WINDOW_FULLSCREEN_DESKTOP)) toggleFullscreen(from);
        return true;
    }
    keyWindow = from;
    const int mods = (sh ? 1 : 0) | (ctrl ? 2 : 0) | (alt ? 4 : 0);
    SDL_Keycode key = normalizeKey(k);
    if (!captureAction.empty()) {  // keyboard editor: "press the key for this action"
        bindKey((int)key, mods, captureAction);
        captureAction.clear();
        return true;
    }
    auto it = keymap.find({(int)key, mods});
    if (it == keymap.end()) {
        // Layouts where the number row types symbols (AZERTY...): fall back to the key's position.
        SDL_Scancode sc = e.keysym.scancode;
        bool printable = k >= 32 && k < 127, letter = k >= 'a' && k <= 'z';
        if (printable && !letter && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) {
            int i = sc - SDL_SCANCODE_1;
            key = i == 9 ? SDLK_0 : SDLK_1 + i;
            it = keymap.find({(int)key, mods});
        }
    }
    if (it == keymap.end()) return false;
    Action* a = actions.find(it->second);
    if (!a) return false;
    if (rep && !a->repeat) return true;
    if (a->kind == Action::Momentary) heldMomentary[a->id] = key;
    actions.invoke(a->id, true);
    return true;
}

void handleKeyUp(const SDL_KeyboardEvent& e) {
    SDL_Keycode key = normalizeKey(e.keysym.sym);
    for (auto it = heldMomentary.begin(); it != heldMomentary.end();) {
        if (it->second == key) { actions.invoke(it->first, false); it = heldMomentary.erase(it); }
        else ++it;
    }
}

void releaseHeldKeys() {
    for (auto& [id, k] : heldMomentary) actions.invoke(id, false);
    heldMomentary.clear();
}


void audioMenuActivate(int i) {
    audioMenuOpen = false;
    if (i < (int)audioDevices.size()) {
        auto& d = audioDevices[i];
        startAudioFrom(d.loopback ? "loopback" : "input", d.name, false);
    } else if (i == (int)audioDevices.size()) playAudioFileDialog();
    else startAudioFrom("none", "", false);
}

// ===================================================================== UI helpers
const ImVec4 kAccent(0.70f, 0.53f, 1.0f, 1.0f);
const ImVec4 kAccent2(0.31f, 0.90f, 0.78f, 1.0f);
const ImVec4 kDim(0.62f, 0.60f, 0.70f, 1.0f);
const ImVec4 kWarn(1, 0.72f, 0.3f, 1);
const ImVec4 kHot(1, 0.36f, 0.48f, 1);

void setupStyle(float scale) {
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 10; st.ChildRounding = 8; st.FrameRounding = 6; st.PopupRounding = 8; st.GrabRounding = 6;
    st.TabRounding = 6; st.ScrollbarRounding = 8; st.WindowBorderSize = 1;
    st.WindowPadding = ImVec2(12, 10);
    st.ItemSpacing = ImVec2(8, 6);
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.08f, 0.82f);
    c[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.10f, 0.96f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.09f, 0.88f);
    c[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.09f);
    c[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.06f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.53f, 1.0f, 0.25f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.07f, 0.12f, 0.92f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.07f, 0.12f, 0.7f);
    c[ImGuiCol_Header] = ImVec4(0.70f, 0.53f, 1.0f, 0.22f);
    c[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.70f, 0.53f, 1.0f, 0.35f);
    c[ImGuiCol_Button] = ImVec4(1, 1, 1, 0.06f);
    c[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 0.12f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.53f, 1.0f, 0.35f);
    c[ImGuiCol_CheckMark] = kAccent2;
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = kAccent2;
    c[ImGuiCol_PlotHistogram] = kAccent2;
    c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_Tab] = ImVec4(1, 1, 1, 0.05f);
    c[ImGuiCol_TabSelected] = ImVec4(0.70f, 0.53f, 1.0f, 0.3f);
    c[ImGuiCol_TabHovered] = ImVec4(0.70f, 0.53f, 1.0f, 0.4f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.70f, 0.53f, 1.0f, 0.2f);
    st.ScaleAllSizes(scale);
}

std::vector<ThumbRenderer::Want> wantThumbs;
void wantThumb(int idx) {
    if (idx < 0 || lib[idx].broken) return;
    for (auto& w : wantThumbs)
        if (w.id == idx) return;
    wantThumbs.push_back({idx, lib[idx].path});
}

void imageFlipped(unsigned tex, ImVec2 size) { ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0, 1), ImVec2(1, 0)); }

void pill(const char* text, ImVec4 col, bool on, const char* tip, std::function<void()> onClick = nullptr) {
    ImGui::PushStyleColor(ImGuiCol_Button, on ? ImVec4(col.x, col.y, col.z, 0.22f) : ImVec4(1, 1, 1, 0.04f));
    ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(1, 1, 1, 1) : kDim);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 99);
    if (ImGui::SmallButton(text) && onClick) onClick();
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
}

std::string stars(int n) {
    std::string s;
    for (int i = 0; i < 5; i++) s += i < n ? "\xE2\x98\x85" : "\xE2\x98\x86";
    return s;
}

void presetContextMenu(int idx) {
    Preset& p = lib[idx];
    Meta& m = meta[p.path];
    ImGui::TextDisabled("%s", p.name.c_str());
    if (ImGui::MenuItem("Load to deck A")) loadPreset(0, idx, false);
    if (ImGui::MenuItem("Load to deck B")) loadPreset(1, idx, false);
    ImGui::Separator();
    if (ImGui::MenuItem("Favorite", "Ctrl+D", m.fav)) { m.fav = !m.fav; saveLibrary(); }
    if (ImGui::BeginMenu("Rating")) {
        for (int r = 0; r <= 5; r++)
            if (ImGui::MenuItem(r ? stars(r).c_str() : "No rating", nullptr, m.rating == r)) { m.rating = r; saveLibrary(); }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tags")) {
        for (size_t i = 0; i < m.tags.size(); i++)
            if (ImGui::MenuItem(("Remove \"" + m.tags[i] + "\"").c_str())) { m.tags.erase(m.tags.begin() + i); saveLibrary(); break; }
        for (auto& t : allTags())
            if (std::find(m.tags.begin(), m.tags.end(), t) == m.tags.end() && ImGui::MenuItem(("Add \"" + t + "\"").c_str())) { m.tags.push_back(t); saveLibrary(); }
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputTextWithHint("##newtag", "new tag + Enter", tagBuf, sizeof tagBuf, ImGuiInputTextFlags_EnterReturnsTrue) && tagBuf[0]) {
            m.tags.push_back(tagBuf);
            tagBuf[0] = 0;
            saveLibrary();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Add to setlist")) {
        for (auto& s : setlists)
            if (ImGui::MenuItem(s.name.c_str())) addToSetlist(idx, s.name);
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputTextWithHint("##newsl", "new setlist + Enter", setlistNameBuf, sizeof setlistNameBuf, ImGuiInputTextFlags_EnterReturnsTrue) && setlistNameBuf[0]) {
            addToSetlist(idx, setlistNameBuf);
            setlistNameBuf[0] = 0;
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    for (int c = 0; c < 10; c++) {
        char lbl[32];
        snprintf(lbl, sizeof lbl, "Store in cue %d", (c + 1) % 10);
        if (ImGui::MenuItem(lbl)) { S.cues[c] = p.path; saveSettings(); }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("More like this", nullptr, false, looks.count(p.path) > 0)) setPool("similar:" + p.path);
    if (ImGui::MenuItem("Edit preset")) { loadPreset(focus, idx, true); openEditor(); }
    if (ImGui::MenuItem("Show in Explorer")) plat::revealInExplorer(p.path);
    if (p.broken && ImGui::MenuItem("Retry")) { p.broken = false; brokenPaths.erase(p.path); }
}

// ===================================================================== panels
void drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Add preset files...", "Ctrl+O")) dialogAddFiles();
        if (ImGui::MenuItem("Add preset folder...", "Ctrl+Shift+O")) dialogAddFolder();
        if (ImGui::MenuItem("Rescan library", "Ctrl+R")) rescan();
        if (ImGui::BeginMenu("Library folders", !S.folders.empty())) {
            for (size_t i = 0; i < S.folders.size(); i++) {
                ImGui::PushID((int)i);
                if (ImGui::MenuItem(("Remove  " + S.folders[i]).c_str())) {
                    S.folders.erase(S.folders.begin() + i);
                    rescan();
                    saveSettings();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Forget single files", nullptr, false, !S.files.empty())) { S.files.clear(); rescan(); saveSettings(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Play audio file...", "M")) playAudioFileDialog();
        if (ImGui::MenuItem("Preset editor", "E")) openEditor();
        ImGui::Separator();
        if (ImGui::MenuItem("Open recordings folder")) { std::string d = plat::videosDir() + "\\Amethyst"; fs::create_directories(fs::u8path(d)); plat::openPath(d); }
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Audio")) {
        refreshDevices();
        ImGui::TextDisabled("System audio (what you hear)");
        for (auto& d : audioDevices)
            if (d.loopback && ImGui::MenuItem((d.name + "##lb").c_str(), d.isDefault ? "default" : nullptr,
                                              audio.source() == AudioEngine::Source::Loopback && audio.deviceName() == d.name))
                startAudioFrom("loopback", d.name, false);
        ImGui::Separator();
        ImGui::TextDisabled("Microphone / line-in");
        for (auto& d : audioDevices)
            if (!d.loopback && ImGui::MenuItem((d.name + "##in").c_str(), d.isDefault ? "default" : nullptr,
                                               audio.source() == AudioEngine::Source::Input && audio.deviceName() == d.name))
                startAudioFrom("input", d.name, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Audio file...", "M", audio.source() == AudioEngine::Source::File)) playAudioFileDialog();
        if (ImGui::MenuItem("Pause / resume file", "Shift+M", false, audio.source() == AudioEngine::Source::File)) audio.togglePause();
        if (ImGui::MenuItem("No audio", nullptr, audio.source() == AudioEngine::Source::None)) startAudioFrom("none", "", false);
        ImGui::Separator();
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Sensitivity", &S.gain, 0.05f, 8.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Output")) {
        int n = SDL_GetNumVideoDisplays();
        for (int i = 0; i < n; i++) {
            SDL_Rect b;
            SDL_GetDisplayBounds(i, &b);
            char label[256];
            snprintf(label, sizeof label, "New fullscreen output on %d: %s (%dx%d)", i + 1, SDL_GetDisplayName(i), b.w, b.h);
            if (ImGui::MenuItem(label)) openOutput(i);
        }
        if (ImGui::MenuItem("New windowed output")) openOutput(-1);
        if (ImGui::MenuItem("Outputs & projection mapping...", nullptr, S.showOutputs)) S.showOutputs = !S.showOutputs;
        ImGui::Separator();
        if (ImGui::MenuItem("Spout sender (to OBS, Resolume...)", "F12", S.spout)) setSpout(!S.spout);
        if (ImGui::MenuItem(recorder.active() ? "Stop recording" : "Record to MP4", "F9", recorder.active())) actions.invoke("record", true);
        if (ImGui::BeginMenu("Recording frame rate")) {
            for (int f : {24, 30, 60})
                if (ImGui::MenuItem((std::to_string(f) + " fps").c_str(), nullptr, S.recFps == f, !recorder.active())) S.recFps = f;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        for (int q = 0; q < 4; q++)
            if (ImGui::MenuItem(kQuality[q].label, q == S.quality ? "Q" : nullptr, S.quality == q)) {
                S.quality = q;
                for (auto& d : decks) for (auto& p : d.players) p.setMesh(kQuality[q].meshW, kQuality[q].meshH);
            }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Control")) {
        if (ImGui::MenuItem("MIDI, OSC & phone remote...", nullptr, S.showControl)) S.showControl = !S.showControl;
        ImGui::Separator();
        if (ImGui::BeginMenu("Tempo source")) {
            if (ImGui::MenuItem("Automatic (detect from audio)", nullptr, tempo.source() == Tempo::Source::Auto)) tempo.setSource(Tempo::Source::Auto);
            if (ImGui::MenuItem("Tap tempo", "T", tempo.source() == Tempo::Source::Tap)) tempo.setSource(Tempo::Source::Tap);
            if (ImGui::MenuItem("Ableton Link", nullptr, tempo.source() == Tempo::Source::Link)) { tempo.enableLink(true); S.link = true; tempo.setSource(Tempo::Source::Link); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Launch quantize")) {
            for (int q = 0; q < 4; q++)
                if (ImGui::MenuItem(kQuantizeNames[q], q == S.quantize ? "K" : nullptr, S.quantize == q)) S.quantize = q;
            ImGui::Separator();
            ImGui::MenuItem("Also quantize manual launches", nullptr, &S.quantizeManual);
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Auto-pilot mixes decks A/B", nullptr, &S.autoDecks);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Menu bar", "F10", &S.showMenu);
        ImGui::MenuItem("Preset browser", "P", &S.showBrowser);
        ImGui::MenuItem("Now-playing bar", "N", &S.showHud);
        ImGui::MenuItem("Cue strip", "C", &S.showCues);
        ImGui::MenuItem("Mixer (decks A/B)", "D", &S.showMixer);
        ImGui::MenuItem("Effects", "W", &S.showFx);
        ImGui::MenuItem("Overlays (logo, text, video)", nullptr, &S.showOverlays);
        ImGui::MenuItem("Outputs & mapping", nullptr, &S.showOutputs);
        ImGui::MenuItem("Setlists", nullptr, &S.showSetlists);
        ImGui::MenuItem("Scenes & show timeline", nullptr, &S.showScenes);
        ImGui::MenuItem("Modulation", nullptr, &S.showMod);
        ImGui::MenuItem("Control (MIDI, OSC, remote)", nullptr, &S.showControl);
        ImGui::MenuItem("Notifications", nullptr, &S.showToasts);
        ImGui::MenuItem("Status in menu bar", nullptr, &S.showStatus);
        ImGui::Separator();
        ImGui::MenuItem("Live previews", "V", &S.livePreviews);
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("Preview size", &S.previewHeight, 36, 160, "%.0f px");
        ImGui::SetNextItemWidth(160);
        if (ImGui::SliderInt("Preview FPS", &S.previewFps, 5, 60)) thumbs.setFps(S.previewFps);
        ImGui::Separator();
        if (ImGui::MenuItem("Hide entire interface", "U")) toggleAllUi();
        if (ImGui::MenuItem("Fullscreen", "F")) toggleFullscreen(win);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Keyboard shortcuts", chordsFor("help").empty() ? nullptr : chordsFor("help")[0].c_str())) helpOpen = true;
        if (ImGui::MenuItem("Customize keyboard layout...", chordsFor("keyboard").empty() ? nullptr : chordsFor("keyboard")[0].c_str())) keyboardOpen = true;
        ImGui::TextDisabled("projectM %s", projectm_get_version_string());
        ImGui::EndMenu();
    }
    if (S.showStatus) {
        char st[200];
        snprintf(st, sizeof st, "%s%d presets   %.0f fps   %dx%d", recorder.active() ? "REC  " : "", (int)lib.size(), ImGui::GetIO().Framerate, renderW, renderH);
        float w = ImGui::CalcTextSize(st).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - w - 16);
        ImGui::TextColored(recorder.active() ? kHot : kDim, "%s", st);
    }
    ImGui::EndMainMenuBar();
}

void drawBrowser(float top, float bottom) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 12, top), ImGuiCond_FirstUseEver, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(460 * io.FontGlobalScale, std::max(200.f, bottom - top)), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Presets", &S.showBrowser, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

    if (focusSearch) { ImGui::SetKeyboardFocusHere(); focusSearch = false; }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search presets...  ( / )", searchBuf, sizeof searchBuf)) { query = searchBuf; rebuildView(); }
    searchHasFocus = ImGui::IsItemActive();

    float half = (ImGui::GetContentRegionAvail().x - 8) * 0.5f;
    ImGui::SetNextItemWidth(half);
    if (ImGui::BeginCombo("##pool", poolLabel().c_str())) {
        if (ImGui::Selectable("All presets", S.pool == "all")) setPool("all");
        if (ImGui::Selectable("Favorites", S.pool == "fav")) setPool("fav");
        ImGui::SeparatorText("By look (auto-analysed)");
        for (auto c : kColorNames)
            if (ImGui::Selectable((std::string("Colour: ") + c).c_str(), S.pool == std::string("color:") + c)) setPool(std::string("color:") + c);
        if (ImGui::Selectable("Calm", S.pool == "energy:calm")) setPool("energy:calm");
        if (ImGui::Selectable("Intense", S.pool == "energy:intense")) setPool("energy:intense");
        if (ImGui::Selectable("Dark", S.pool == "light:dark")) setPool("light:dark");
        if (ImGui::Selectable("Bright", S.pool == "light:bright")) setPool("light:bright");
        if (S.pool.rfind("similar:", 0) == 0) ImGui::Selectable(poolLabel().c_str(), true);
        ImGui::SeparatorText("Library");
        for (int r = 3; r <= 5; r++)
            if (ImGui::Selectable((stars(r) + (r < 5 ? " and up" : "")).c_str(), S.pool == "rating:" + std::to_string(r))) setPool("rating:" + std::to_string(r));
        for (auto& t : allTags())
            if (ImGui::Selectable(("Tag: " + t).c_str(), S.pool == "tag:" + t)) setPool("tag:" + t);
        for (auto& s : setlists)
            if (ImGui::Selectable(("Setlist: " + s.name).c_str(), S.pool == "setlist:" + s.name)) setPool("setlist:" + s.name);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(half);
    if (ImGui::BeginCombo("##src", sourceFilter < 0 ? "All folders" : sourceNames[sourceFilter].c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("All folders", sourceFilter < 0)) { sourceFilter = -1; rebuildView(); }
        for (int i = 0; i < (int)sourceNames.size(); i++)
            if (ImGui::Selectable(sourceNames[i].c_str(), sourceFilter == i)) { sourceFilter = i; rebuildView(); }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Live previews", &S.livePreviews);
    ImGui::SameLine();
    if (ImGui::Checkbox("Hide broken", &S.hideBroken)) rebuildView();
    ImGui::SameLine();
    {
        int pend = thumbs.analysisPending();
        if (pend > 0 && S.analysisOn) ImGui::TextDisabled("analysing %d...", pend);
        else ImGui::TextDisabled("%d analysed", (int)looks.size());
        if (pend > 0 && S.analysisOn && !thumbs.analysisEnabled) { ImGui::SameLine(); ImGui::TextDisabled("(paused while performing)"); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Presets are analysed in the background (colour, brightness, motion)\nfor the colour/energy pools and \"More like this\". Right-click to pause.");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { S.analysisOn = !S.analysisOn; thumbs.analysisEnabled = S.analysisOn; }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d shown  -  loads into deck %s%s", (int)view.size(), decks[focus].name, keyHint("focus_toggle").c_str());

    if (lib.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("No presets yet. Drop .milk files or a whole folder onto the window, or:");
        if (ImGui::Button("Add preset folder...")) dialogAddFolder();
        ImGui::End();
        return;
    }

    const float th = S.previewHeight * io.FontGlobalScale;
    const float tw = th * 16.f / 9.f;
    const float rowH = th + 6;
    ImGui::BeginChild("list", ImVec2(0, 0), ImGuiChildFlags_None);
    if (scrollToSel && sel >= 0) {
        float y = sel * rowH, scr = ImGui::GetScrollY(), h = ImGui::GetWindowHeight();
        if (y < scr) ImGui::SetScrollY(y);
        else if (y + rowH > scr + h) ImGui::SetScrollY(y + rowH - h);
        scrollToSel = false;
    }
    ImGuiListClipper clip;
    clip.Begin((int)view.size(), rowH);
    while (clip.Step()) {
        for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
            int idx = view[r];
            Preset& p = lib[idx];
            ImGui::PushID(idx);
            ImVec2 rowPos = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##row", r == sel, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowH - 2))) {
                sel = r;
                if (io.KeyCtrl) loadPreset(1 - focus, idx, io.KeyShift);
                else loadPreset(focus, idx, io.KeyShift);
            }
            if (ImGui::BeginPopupContextItem()) { presetContextMenu(idx); ImGui::EndPopup(); }
            if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s\nClick: deck %s   Ctrl+click: deck %s   Shift: cut", p.path.c_str(), decks[focus].name, decks[1 - focus].name);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 tp(rowPos.x + 2, rowPos.y + 2), te(tp.x + tw, tp.y + th - 2);
            unsigned tex = S.livePreviews ? thumbs.textureFor(idx) : 0;
            if (tex) dl->AddImageRounded((ImTextureID)(intptr_t)tex, tp, te, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, 5);
            else dl->AddRectFilled(tp, te, IM_COL32(255, 255, 255, 12), 5);
            wantThumb(idx);
            bool onA = idx == decks[0].cur, onB = idx == decks[1].cur;
            ImU32 col = p.broken ? IM_COL32(130, 126, 150, 255) : (onA || onB) ? ImGui::GetColorU32(kAccent2) : IM_COL32(236, 235, 245, 255);
            float tx = rowPos.x + tw + 12;
            float lh = ImGui::GetTextLineHeight();
            ImVec4 clipR(tx, rowPos.y, rowPos.x + ImGui::GetWindowContentRegionMax().x - 4, rowPos.y + rowH);
            dl->AddText(nullptr, 0, ImVec2(tx, rowPos.y + rowH * 0.5f - lh), col, p.name.c_str(), nullptr, 0, &clipR);
            auto mi = meta.find(p.path);
            std::string info;
            if (onA) info += "[A] ";
            if (onB) info += "[B] ";
            if (mi != meta.end()) {
                if (mi->second.fav) info += "\xE2\x99\xA5 ";
                if (mi->second.rating) info += stars(mi->second.rating) + " ";
                for (auto& t : mi->second.tags) info += "#" + t + " ";
            }
            info += p.source;
            dl->AddText(nullptr, 0, ImVec2(tx, rowPos.y + rowH * 0.5f + 1), ImGui::GetColorU32(kDim), info.c_str(), nullptr, 0, &clipR);
            for (int c = 0; c < 10; c++)
                if (S.cues[c] == p.path) {
                    char b[4];
                    snprintf(b, sizeof b, "%d", (c + 1) % 10);
                    dl->AddText(ImVec2(rowPos.x + 6, rowPos.y + 4), ImGui::GetColorU32(kAccent), b);
                }
            ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + rowH));
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void drawCues(float y, float h) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12, y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 24, h), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cues", &S.showCues, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) { ImGui::End(); return; }
    float avail = ImGui::GetContentRegionAvail().x;
    float cw = (avail - 9 * 6) / 10.f;
    float chh = std::max(24.f, ImGui::GetContentRegionAvail().y - 2);
    for (int i = 0; i < 10; i++) {
        ImGui::PushID(i);
        auto it = S.cues[i].empty() ? byPath.end() : byPath.find(S.cues[i]);
        int idx = it == byPath.end() ? -1 : it->second;
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("cue", ImVec2(cw, chh))) {
            if (io.KeyShift) storeCue(i);
            else if (io.KeyAlt) clearCue(i);
            else fireCue(i);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) clearCue(i);
        if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s", idx >= 0 ? lib[idx].name.c_str() : "Empty - Shift+click or Shift+number stores the current preset");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool active = idx >= 0 && (idx == decks[0].cur || idx == decks[1].cur);
        dl->AddRectFilled(p, ImVec2(p.x + cw, p.y + chh), active ? IM_COL32(179, 136, 255, 60) : IM_COL32(255, 255, 255, 10), 7);
        float textX = p.x + 22;
        if (idx >= 0) {
            unsigned tex = S.livePreviews ? thumbs.textureFor(idx) : 0;
            float ih = chh - 4, iw = std::min(cw * 0.45f, ih * 16 / 9.f);
            if (tex) dl->AddImageRounded((ImTextureID)(intptr_t)tex, ImVec2(p.x + 18, p.y + 2), ImVec2(p.x + 18 + iw, p.y + 2 + ih), ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, 5);
            wantThumb(idx);
            textX = p.x + 24 + iw;
            ImVec4 clipR(textX, p.y, p.x + cw - 4, p.y + chh);
            dl->AddText(nullptr, 0, ImVec2(textX, p.y + chh * 0.5f - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(236, 235, 245, 255),
                        lib[idx].name.c_str(), nullptr, 0, &clipR);
        }
        char num[4];
        snprintf(num, sizeof num, "%d", (i + 1) % 10);
        dl->AddText(ImVec2(p.x + 6, p.y + 3), ImGui::GetColorU32(kAccent), num);
        if (active) dl->AddRect(p, ImVec2(p.x + cw, p.y + chh), ImGui::GetColorU32(kAccent), 7, 0, 1.5f);
        if (i < 9) ImGui::SameLine(0, 6);
        ImGui::PopID();
    }
    ImGui::End();
}

void drawHud(float y, float h) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12, y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 24, h), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Now playing", &S.showHud, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) { ImGui::End(); return; }

    float rightW = 340 * io.FontGlobalScale;
    int fc = decks[focus].cur;
    ImGui::BeginGroup();
    ImGui::PushFont(fontBig);
    ImGui::TextUnformatted(fc >= 0 ? lib[fc].name.c_str() : "No preset");
    ImGui::PopFont();
    int other = decks[1 - focus].cur;
    auto it = std::find(view.begin(), view.end(), fc);
    std::string sub = "Deck " + std::string(decks[focus].name);
    if (fc >= 0 && it != view.end()) sub += "  #" + std::to_string(it - view.begin() + 1) + " of " + std::to_string(view.size());
    if (S.pool != "all") sub += "  -  " + poolLabel();
    if (!query.empty()) sub += "  -  search: " + query;
    if (S.xfade > 0.02f && other >= 0) sub += "   |   deck " + std::string(decks[1 - focus].name) + ": " + lib[other].name;
    ImGui::TextDisabled("%s", sub.c_str());
    ImGui::EndGroup();

    // tempo + level (right side)
    ImGui::SameLine(ImGui::GetWindowWidth() - rightW - 12);
    ImGui::BeginGroup();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    double beat = beatNow();
    int bi = ((int)std::floor(beat) % 4 + 4) % 4;
    float frac = (float)(beat - std::floor(beat));
    ImVec2 p = ImGui::GetCursorScreenPos();
    for (int i = 0; i < 4; i++) {
        ImU32 c = i == bi ? IM_COL32(255, 110, 199, (int)(255 - 180 * frac)) : IM_COL32(255, 255, 255, 30);
        dl->AddCircleFilled(ImVec2(p.x + 7 + i * 16, p.y + 9), 6, c);
    }
    ImGui::Dummy(ImVec2(66, 18));
    ImGui::SameLine();
    char bpm[64];
    snprintf(bpm, sizeof bpm, "%.1f BPM  %s%s", tempo.bpm(), tempo.sourceName(),
             tempo.source() == Tempo::Source::Link ? (" (" + std::to_string(tempo.linkPeers()) + " peers)").c_str() : "");
    if (ImGui::SmallButton(bpm)) tempo.tap(now());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click or press T to tap.  Shift+T: Auto / Tap / Link");
    ImGui::ProgressBar(std::clamp(level * 3.f, 0.f, 1.f), ImVec2(rightW - 10, 8), "");
    ImGui::TextDisabled("%s%s", audio.label().c_str(), audio.paused() ? "  (paused)" : "");
    ImGui::EndGroup();

    char b[128];
    double since = now() - decks[S.xfade < 0.5f ? 0 : 1].lastSwitch;
    double left = std::max(0.0, S.interval - since);
    double hb = beatNow();
    if (S.autoOn && S.smartMix)
        snprintf(b, sizeof b, "AUTO smart  %s  bar %d/8%s%s", MusicStructure::name(structure.section()), structure.barInPhrase(hb) + 1,
                 S.autoDecks ? ", A/B mix" : "", keyHint("auto").c_str());
    else if (S.autoOn) snprintf(b, sizeof b, "AUTO %s  %s%s%s", locked ? "--" : (std::to_string((int)std::ceil(left)) + "s").c_str(), S.shuffle ? "shuffle" : "in order", S.autoDecks ? ", A/B mix" : "", keyHint("auto").c_str());
    else snprintf(b, sizeof b, "AUTO off%s", keyHint("auto").c_str());
    pill(b, kAccent2, S.autoOn, "Auto-pilot (A). Interval , .  Shuffle S.  Quantize K", [] { actions.invoke("auto", true); });
    if (locked) pill(("LOCKED" + keyHint("lock")).c_str(), kWarn, true, nullptr, [] { locked = false; });
    snprintf(b, sizeof b, "QUANTIZE %s%s", kQuantizeNames[S.quantize], keyHint("quantize").c_str()); pill(b, kAccent, S.quantize > 0, "Launches wait for the next beat/bar", [] { actions.invoke("quantize", true); });
    snprintf(b, sizeof b, "BLEND %.2gs [ ]", S.blend); pill(b, kAccent, false, "Blend time");
    snprintf(b, sizeof b, "EVERY %ds , .", S.interval); pill(b, kAccent, false, "Auto-pilot interval");
    snprintf(b, sizeof b, "MIX %s %.0f%%%s", mixModeName(S.mixMode), S.xfade * 100, keyHint("mix_mode").c_str()); pill(b, kAccent, S.xfade > 0.01f, "Crossfader: Ctrl+Left/Right, \\ auto-fades", [] { actions.invoke("xfade_auto", true); });
    snprintf(b, sizeof b, "SPEED %.2gx", S.speed); pill(b, kWarn, S.speed != 1, "- = (Backspace resets)", [] { S.speed = 1; });
    snprintf(b, sizeof b, "GAIN %.2gx", S.gain); pill(b, kAccent, false, "Audio sensitivity (Up/Down)");
    snprintf(b, sizeof b, "%s%s", kQuality[S.quality].label, keyHint("quality").c_str()); pill(b, kAccent, false, "Render quality");
    int fxOn = S.fx.kaleido + S.fx.mirror + S.fx.feedback + S.fx.rgbSplit + S.fx.pixelate + S.fx.invert + S.fx.strobe;
    if (fxOn) { snprintf(b, sizeof b, "FX x%d%s", fxOn, keyHint("panel_fx").c_str()); pill(b, kAccent2, true, "Effects panel (W)", [] { S.showFx = true; }); }
    if (!outputs.empty()) { snprintf(b, sizeof b, "OUTPUT x%d", (int)outputs.size()); pill(b, kAccent2, true, nullptr, [] { S.showOutputs = true; }); }
    if (S.spout) pill("SPOUT", kAccent2, true, nullptr, [] { setSpout(false); });
    if (recorder.active()) { snprintf(b, sizeof b, "REC %02d:%02d%s", (int)recorder.seconds() / 60, (int)recorder.seconds() % 60, keyHint("record").c_str()); pill(b, kHot, true, nullptr, [] { stopRecording(); }); }
    if (blackoutOn) pill(("BLACKOUT" + keyHint("blackout")).c_str(), kHot, true, nullptr, [] { blackoutOn = false; });
    if (frozen) pill(("FROZEN" + keyHint("freeze")).c_str(), kWarn, true, nullptr);
    if (launch.armed) pill("LAUNCH QUEUED", kWarn, true, "Waiting for the next beat/bar");
    if (!midi.learning.empty()) pill("MIDI LEARN...", kWarn, true, "Move a control on your MIDI device (Esc cancels)");
    ImGui::NewLine();
    ImGui::End();
}

void deckColumn(int d, float w) {
    Deck& dk = decks[d];
    ImGui::BeginGroup();
    ImGui::PushID(d);
    bool foc = focus == d;
    ImGui::TextColored(foc ? kAccent2 : kDim, "DECK %s%s", dk.name, foc ? "  (focus)" : "");
    float h = w * 9 / 16.f;
    if (dk.outTex) imageFlipped(dk.outTex, ImVec2(w, h));
    else ImGui::Dummy(ImVec2(w, h));
    if (ImGui::IsItemClicked()) focus = d;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextUnformatted(dk.cur >= 0 ? lib[dk.cur].name.c_str() : "-");
    ImGui::PopTextWrapPos();
    if (ImGui::Button("< Prev")) back(d, false);
    ImGui::SameLine();
    if (ImGui::Button("Next >")) go(d, 1, false);
    ImGui::SameLine();
    if (ImGui::Button("Random")) go(d, 1, false, true);
    ImGui::SameLine();
    if (ImGui::Button("Focus")) focus = d;
    ImGui::PopID();
    ImGui::EndGroup();
}

void drawLightingTab();
void drawDjTab();
void drawLayerSource();
bool selectDjTab = false;

void drawMixer() {
    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Mixer", &S.showMixer, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    float w = (ImGui::GetContentRegionAvail().x - 16) / 2;
    deckColumn(0, w);
    ImGui::SameLine(0, 16);
    deckColumn(1, w);
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##xf", &S.xfade, 0, 1, "A  <-  crossfader  ->  B")) xanim.active = false;
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Mix mode", mixModeName(S.mixMode))) {
        for (int m = 0; m < MixModeCount; m++)
            if (ImGui::Selectable(mixModeName(m), S.mixMode == m)) S.mixMode = m;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto crossfade  ( \\ )")) actions.invoke("xfade_auto", true);
    ImGui::Checkbox("Auto-pilot mixes A/B (loads the next preset on the hidden deck, then crossfades)", &S.autoDecks);
    ImGui::Checkbox("Smart auto-mix: follow the song", &S.smartMix);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Switches on phrase boundaries, cuts to an intense preset exactly on the drop,\n"
                          "goes calm with a long blend in breakdowns, and changes when a new song starts.");
    if (S.smartMix) {
        ImGui::Checkbox("After a fakeout, switch again on the real drop", &S.fakeoutRecut);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Some songs drop for a bar or two, cut out, then drop for real.\nOn: the real drop gets its own fresh preset. Off: keep the one from the fake drop.");
        double hb = beatNow();
        ImGui::TextDisabled("Now: %s   bar %d/8, beat %d   energy %.2f   last: %s", MusicStructure::name(structure.section()),
                            structure.barInPhrase(hb) + 1, structure.beatInBar(hb) + 1, structure.energy(),
                            smart.lastEvent.empty() ? "-" : smart.lastEvent.c_str());
    }
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("Transition", transitionName(S.transition))) {
        for (int k = 0; k < TransitionCount; k++)
            if (ImGui::Selectable(transitionName(k), S.transition == k)) S.transition = k;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170);
    ImGui::SliderFloat("##tbeats", &S.transBeats, 0, 32, S.transBeats < 0.01f ? "length: blend time" : "length: %.0f beats");
    ImGui::TextDisabled("Transitions play on auto-crossfades and A/B auto-pilot, timed to the beat clock.");
    ImGui::TextDisabled("Tab switches focus. Ctrl+click a preset loads the other deck. Ctrl+Left/Right nudges the fader.");
    ImGui::End();
}

void drawFx() {
    ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Effects", &S.showFx, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    FxParams& f = S.fx;
    ImGui::Checkbox("Kaleidoscope  F2", &f.kaleido); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##ks", &f.kaleidoSegments, 2, 16, "%d segments");
    ImGui::Checkbox("Mirror  F3", &f.mirror); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::Combo("##mm", &f.mirrorMode, "Off\0Horizontal\0Vertical\0Quad\0");
    ImGui::Checkbox("Feedback trails  F4", &f.feedback); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##fa", &f.feedbackAmount, 0.3f, 0.98f, "amount %.2f");
    if (f.feedback) {
        ImGui::Indent(); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##fz", &f.feedbackZoom, 0.9f, 1.15f, "trail zoom %.3f");
        ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##fr", &f.feedbackRotate, -0.1f, 0.1f, "trail rotate %.3f"); ImGui::Unindent();
    }
    ImGui::Checkbox("RGB split  F5", &f.rgbSplit); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##rs", &f.rgbSplitAmount, 0.001f, 0.05f, "%.3f");
    ImGui::Checkbox("Pixelate  F6", &f.pixelate); ImGui::SameLine(220); ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##px", &f.pixelSize, 2, 64, "%.0f px");
    ImGui::Checkbox("Invert  F7", &f.invert);
    ImGui::Checkbox("Strobe  F8", &f.strobe); ImGui::SameLine(220); ImGui::Checkbox("on beat", &f.strobeOnBeat);
    ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##sr", &f.strobeRate, 1, 16, f.strobeOnBeat ? "%.0f per bar" : "%.1f Hz");
    ImGui::TextColored(kWarn, "Strobe can trigger photosensitive reactions — use with care.");
    ImGui::Separator();
    ImGui::TextDisabled("Transform");
    ImGui::SliderFloat("Zoom", &f.zoom, 0.4f, 4.f, "%.2fx", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Rotate speed", &f.rotateSpeed, -2, 2, "%.2f rad/s");
    ImGui::SliderFloat("Swirl", &f.swirl, -2, 2, "%.2f");
    ImGui::Separator();
    ImGui::TextDisabled("Color");
    ImGui::SliderFloat("Hue shift", &f.hue, 0, 1, "%.2f");
    ImGui::SliderFloat("Saturation", &f.saturation, 0, 2.5f, "%.2f");
    ImGui::SliderFloat("Contrast", &f.contrast, 0.3f, 2.5f, "%.2f");
    ImGui::SliderFloat("Brightness", &f.brightness, -0.5f, 0.5f, "%.2f");
    ImGui::Separator();
    if (ImGui::Button("Reset all effects")) actions.invoke("fx_reset", true);
    ImGui::End();
}

void drawOverlaysPanel() {
    ImGui::SetNextWindowSize(ImVec2(440, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Overlays", &S.showOverlays, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::TextDisabled("Drop an image or video file on the window to use it here.");
    if (ImGui::CollapsingHeader("Video layer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("video");
        drawLayerSource();
        if (ImGui::Checkbox("Show", &S.videoOn) && S.videoOn && !video.active() && !S.videoPath.empty()) openVideo(S.videoPath);
        ImGui::SameLine();
        if (ImGui::Button("Choose video...")) {
            auto f = plat::pickFiles(hwndOf(win), L"Choose a video", {{L"Video", L"*.mp4;*.mov;*.wmv;*.avi;*.mkv;*.m4v"}}, false);
            if (!f.empty()) openVideo(f[0]);
        }
        ImGui::SameLine();
        bool p = video.paused;
        if (ImGui::Checkbox("Pause", &p)) video.paused = p;
        ImGui::TextDisabled("%s", video.active() ? video.name().c_str() : "(none)");
        ImGui::SliderFloat("Opacity", &S.videoOpacity, 0, 1);
        ImGui::Combo("Blend", &S.videoBlend, "Normal\0Add\0Screen\0Multiply\0");
        ImGui::Combo("Fit", &S.videoFit, "Fill\0Fit\0Stretch\0");
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Logo / image", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("logo");
        ImGui::Checkbox("Show", &S.logoOn);
        ImGui::SameLine();
        if (ImGui::Button("Choose image...")) {
            auto f = plat::pickFiles(hwndOf(win), L"Choose a logo", {{L"Images", L"*.png;*.jpg;*.jpeg"}}, false);
            if (!f.empty()) { S.logoPath = f[0]; S.logoOn = true; }
        }
        ImGui::TextDisabled("%s", S.logoPath.empty() ? "(none)" : fs::u8path(S.logoPath).filename().u8string().c_str());
        ImGui::Combo("Position", &S.logoPos, "Top left\0Top right\0Bottom left\0Bottom right\0Center\0");
        ImGui::SliderFloat("Size", &S.logoScale, 0.03f, 1.0f, "%.2f of width");
        ImGui::SliderFloat("Opacity", &S.logoOpacity, 0, 1);
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("text");
        ImGui::Checkbox("Show", &S.textOn);
        static char buf[512];
        if (strcmp(buf, S.text.c_str()) != 0 && !ImGui::IsAnyItemActive()) snprintf(buf, sizeof buf, "%s", S.text.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##t", "Text / lyrics / ticker", buf, sizeof buf)) S.text = buf;
        ImGui::Combo("Position", &S.textPos, "Top\0Center\0Bottom\0");
        ImGui::SliderFloat("Size", &S.textSize, 0.02f, 0.4f, "%.2f of height");
        ImGui::SliderFloat("Scroll speed", &S.textScroll, 0, 3, S.textScroll > 0 ? "%.2f" : "static");
        float col[3] = {S.textR, S.textG, S.textB};
        if (ImGui::ColorEdit3("Color", col)) { S.textR = col[0]; S.textG = col[1]; S.textB = col[2]; }
        ImGui::SliderFloat("Opacity", &S.textOpacity, 0, 1);
        ImGui::PopID();
    }
    ImGui::End();
}

void drawOutputs() {
    ImGui::SetNextWindowSize(ImVec2(560, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Outputs & mapping", &S.showOutputs, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    int n = SDL_GetNumVideoDisplays();
    if (ImGui::Button("+ Fullscreen output")) ImGui::OpenPopup("adddisp");
    if (ImGui::BeginPopup("adddisp")) {
        for (int i = 0; i < n; i++)
            if (ImGui::MenuItem((std::to_string(i + 1) + ": " + SDL_GetDisplayName(i)).c_str())) openOutput(i);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Windowed output")) openOutput(-1);
    ImGui::SameLine();
    if (ImGui::Checkbox("Spout", &S.spout)) setSpout(S.spout);
    ImGui::SameLine();
    if (ImGui::Button(recorder.active() ? "Stop recording" : "Record MP4")) actions.invoke("record", true);
    if (ImGui::CollapsingHeader("Lighting (Art-Net / DMX)", S.dmxOn ? ImGuiTreeNodeFlags_DefaultOpen : 0)) drawLightingTab();
    if (outputs.empty()) {
        ImGui::TextWrapped("No outputs open. Add one per projector or screen. Each output can show the final mix or a single deck, and can be corner-pinned to fit angled screens.");
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabBar("outs")) {
        for (int i = 0; i < (int)outputs.size(); i++) {
            Output& o = outputs[i];
            char tab[32];
            snprintf(tab, sizeof tab, "Output %d", i + 1);
            if (!ImGui::BeginTabItem(tab)) continue;
            outputSel = i;
            ImGui::SetNextItemWidth(200);
            ImGui::Combo("Shows", &o.source, kOutputSources, IM_ARRAYSIZE(kOutputSources));
            ImGui::SameLine();
            ImGui::Checkbox("Test grid", &o.grid);
            ImGui::SameLine();
            if (ImGui::Button("Reset corners")) { float d[8] = {0, 0, 1, 0, 1, 1, 0, 1}; memcpy(o.c, d, sizeof d); }
            ImGui::SameLine();
            if (ImGui::Button("Fullscreen")) toggleFullscreen(o.win);
            ImGui::SameLine();
            bool close = ImGui::Button("Close");
            // corner-pin editor
            int ow, oh;
            SDL_GetWindowSize(o.win, &ow, &oh);
            float aw = ImGui::GetContentRegionAvail().x, ah = aw * oh / (float)std::max(1, ow);
            ah = std::min(ah, ImGui::GetContentRegionAvail().y - 30);
            aw = ah * ow / (float)std::max(1, oh);
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("canvas", ImVec2(aw, ah));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + aw, p0.y + ah), IM_COL32(0, 0, 0, 200));
            ImVec2 pts[4];
            for (int k = 0; k < 4; k++) pts[k] = ImVec2(p0.x + o.c[k * 2] * aw, p0.y + o.c[k * 2 + 1] * ah);
            dl->AddImageQuad((ImTextureID)(intptr_t)outputTexture(o.source), pts[0], pts[1], pts[2], pts[3], ImVec2(0, 1), ImVec2(1, 1), ImVec2(1, 0), ImVec2(0, 0));
            dl->AddPolyline(pts, 4, ImGui::GetColorU32(kAccent), ImDrawFlags_Closed, 2);
            static int dragK = -1;
            for (int k = 0; k < 4; k++) {
                ImGui::PushID(k);
                ImGui::SetCursorScreenPos(ImVec2(pts[k].x - 9, pts[k].y - 9));
                ImGui::InvisibleButton("h", ImVec2(18, 18));
                if (ImGui::IsItemActive()) dragK = k;
                bool hot = ImGui::IsItemHovered() || dragK == k;
                dl->AddCircleFilled(pts[k], hot ? 9.f : 7.f, hot ? ImGui::GetColorU32(kAccent2) : IM_COL32(255, 255, 255, 220));
                ImGui::PopID();
            }
            if (dragK >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 m = ImGui::GetIO().MousePos;
                o.c[dragK * 2] = std::clamp((m.x - p0.x) / aw, -0.5f, 1.5f);
                o.c[dragK * 2 + 1] = std::clamp((m.y - p0.y) / ah, -0.5f, 1.5f);
            } else if (dragK >= 0) { dragK = -1; saveSettings(); }
            ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + ah + 4));
            ImGui::TextDisabled("Drag the corners to fit the projected image onto your surface.");
            ImGui::EndTabItem();
            if (close) { closeOutput(i); break; }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void drawControl() {
    ImGui::SetNextWindowSize(ImVec2(620, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Control", &S.showControl, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    if (ImGui::BeginTabBar("ctl")) {
        if (ImGui::BeginTabItem("MIDI")) {
            ImGui::Text("Devices: ");
            ImGui::SameLine();
            if (midi.devices().empty()) ImGui::TextDisabled("none found");
            for (auto& d : midi.devices()) { ImGui::SameLine(); ImGui::TextColored(kAccent2, "%s", d.c_str()); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Rescan")) midi.open();
            ImGui::TextDisabled("Last message: %s", midi.lastEvent.empty() ? "-" : midi.lastEvent.c_str());
            ImGui::TextWrapped("Click Learn, then move a knob/fader or hit a pad. Knobs and faders work best on sliders; pads on buttons.");
            if (ImGui::BeginTable("map", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableSetupColumn("Mapping", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                for (auto& a : actions.all()) {
                    ImGui::PushID(a.id.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(a.label.c_str());
                    if (a.kind == Action::Continuous) { ImGui::SameLine(); ImGui::TextDisabled("(knob)"); }
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", a.group.c_str());
                    ImGui::TableNextColumn();
                    std::string m;
                    for (auto& b : midi.bindings)
                        if (b.action == a.id) m += (m.empty() ? "" : ", ") + Midi::describe(b);
                    ImGui::TextUnformatted(midi.learning == a.id ? "move a control..." : m.empty() ? "-" : m.c_str());
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton(midi.learning == a.id ? "Cancel" : "Learn")) midi.learning = midi.learning == a.id ? "" : a.id;
                    if (!m.empty()) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Clear")) {
                            midi.bindings.erase(std::remove_if(midi.bindings.begin(), midi.bindings.end(), [&](const MidiBinding& b) { return b.action == a.id; }), midi.bindings.end());
                            saveSettings();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Phone remote")) {
            bool on = S.webOn;
            if (ImGui::Checkbox("Enable web remote", &on)) { S.webOn = on; applyNetSettings(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Port##web", &S.webPort, 0) && S.webOn) applyNetSettings();
            ImGui::TextWrapped("Open one of these addresses on a phone or tablet on the same Wi-Fi. Anyone on your network can use it while it's on; Windows may ask to allow the app through the firewall.");
            if (web.running())
                for (auto& ip : HttpServer::localAddresses()) {
                    std::string url = "http://" + ip + ":" + std::to_string(S.webPort) + "/";
                    ImGui::TextColored(kAccent2, "%s", url.c_str());
                    ImGui::SameLine();
                    ImGui::PushID(url.c_str());
                    if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(url.c_str());
                    ImGui::PopID();
                }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("DJ link", nullptr, selectDjTab ? ImGuiTabItemFlags_SetSelected : 0)) {
            selectDjTab = false;
            drawDjTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("OSC")) {
            bool on = S.oscOn;
            if (ImGui::Checkbox("Enable OSC (UDP)", &on)) { S.oscOn = on; applyNetSettings(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Port##osc", &S.oscPort, 0) && S.oscOn) applyNetSettings();
            ImGui::TextDisabled("Last: %s", osc.lastMessage.empty() ? "-" : osc.lastMessage.c_str());
            ImGui::TextWrapped("Send /amethyst/<action> with an optional float (0..1 for sliders, 1/0 for buttons). Examples: /amethyst/next, /amethyst/crossfade 0.5, /amethyst/fx_kaleido, /amethyst/cue/3, /amethyst/tap");
            if (ImGui::BeginTable("osc", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
                for (auto& a : actions.all()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(kAccent2, "/amethyst/%s", a.id.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s%s", a.label.c_str(), a.kind == Action::Continuous ? " (0..1)" : "");
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void drawSetlists() {
    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Setlists", &S.showSetlists, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputTextWithHint("##new", "New setlist name", setlistNameBuf, sizeof setlistNameBuf, ImGuiInputTextFlags_EnterReturnsTrue) && setlistNameBuf[0]) {
        setlists.push_back({setlistNameBuf, {}});
        S.activeSetlist = setlistNameBuf;
        setlistNameBuf[0] = 0;
        saveLibrary();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+L adds the current preset");
    for (size_t si = 0; si < setlists.size(); si++) {
        Setlist& s = setlists[si];
        ImGui::PushID((int)si);
        bool active = S.activeSetlist == s.name;
        bool open = ImGui::CollapsingHeader((s.name + "  (" + std::to_string(s.items.size()) + ")" + (active ? "  - active" : "") + "###sl").c_str());
        if (open) {
            if (ImGui::SmallButton("Play as pool")) { setPool("setlist:" + s.name); S.shuffle = false; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Make active")) S.activeSetlist = s.name;
            ImGui::SameLine();
            if (ImGui::SmallButton("Add current")) addToSetlist(decks[focus].cur, s.name);
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                if (S.pool == "setlist:" + s.name) setPool("all");
                setlists.erase(setlists.begin() + si);
                saveLibrary();
                ImGui::PopID();
                break;
            }
            for (size_t i = 0; i < s.items.size(); i++) {
                ImGui::PushID((int)i);
                auto it = byPath.find(s.items[i]);
                std::string nm = it == byPath.end() ? "(missing) " + s.items[i] : lib[it->second].name;
                ImGui::Selectable((std::to_string(i + 1) + ". " + nm).c_str(), false, ImGuiSelectableFlags_AllowOverlap);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && it != byPath.end()) loadPreset(focus, it->second, false);
                if (ImGui::BeginDragDropSource()) { int k = (int)i; ImGui::SetDragDropPayload("SLITEM", &k, sizeof k); ImGui::TextUnformatted(nm.c_str()); ImGui::EndDragDropSource(); }
                if (ImGui::BeginDragDropTarget()) {
                    if (auto* pl = ImGui::AcceptDragDropPayload("SLITEM")) {
                        int from = *(const int*)pl->Data;
                        std::string item = s.items[from];
                        s.items.erase(s.items.begin() + from);
                        s.items.insert(s.items.begin() + i, item);
                        saveLibrary();
                        if (S.pool == "setlist:" + s.name) rebuildView();
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine(ImGui::GetContentRegionMax().x - 20);
                if (ImGui::SmallButton("x")) { s.items.erase(s.items.begin() + i); saveLibrary(); ImGui::PopID(); break; }
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    ImGui::End();
}

int editorResize(ImGuiInputTextCallbackData* d) {
    if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* s = (std::string*)d->UserData;
        s->resize(d->BufTextLen);
        d->Buf = s->data();
    }
    return 0;
}

void drawEditor() {
    ImGui::SetNextWindowSize(ImVec2(760, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Preset editor", &editorOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    static bool live = true;
    auto edited = [&] {
        editorDirty = true;
        if (live) editorApplyAt = now() + 0.15;  // debounce: compile once the user pauses
    };
    ImGuiIO& io = ImGui::GetIO();
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
    ImGui::BeginDisabled(!visualEditor.canUndo());
    if ((ImGui::Button("Undo") || (focused && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))) &&
        visualEditor.undo(editorText))
        edited();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!visualEditor.canRedo());
    if ((ImGui::Button("Redo") || (focused && io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))))) &&
        visualEditor.redo(editorText))
        edited();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Revert") && visualEditor.revert(editorText)) edited();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go back to the preset as it was when you opened it.");
    ImGui::SameLine();
    if (ImGui::Button("Save as...  Ctrl+S")) saveEditorAs();
    ImGui::SameLine();
    if (ImGui::Button("Edit current preset")) openEditor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load the preset now playing on the focused deck.");
    ImGui::SameLine();
    ImGui::Checkbox("Live", &live);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show every change on screen straight away.");
    if (!live) {
        ImGui::SameLine();
        if (ImGui::Button("Apply  Ctrl+Enter")) applyEditor();
    }
    ImGui::TextDisabled("%s%s", editorStatus.c_str(), editorDirty ? "  (modified)" : "");
    if (editorText.empty()) ImGui::TextWrapped("Pick a preset first, then press \"Edit current preset\".");
    if (ImGui::BeginTabBar("##edtabs")) {
        if (visualEditor.draw(editorText)) edited();
        if (ImGui::BeginTabItem("Code (advanced)")) {
            editorText.reserve(editorText.size() + 1);
            if (ImGui::InputTextMultiline("##code", editorText.data(), editorText.capacity() + 1, ImVec2(-1, -1),
                                          ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput, editorResize, &editorText))
                edited();
            if (ImGui::IsItemDeactivatedAfterEdit()) visualEditor.external(editorText);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (editorApplyAt > 0 && now() >= editorApplyAt) {
        editorApplyAt = 0;
        applyEditor();
    }
    ImGui::End();
}

void drawToasts(float top) {
    double t = now();
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(), [&](const Toast& x) { return x.until < t; }), toasts.end());
    if (toasts.empty() || !S.showToasts) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, top + 10), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::Begin("##toasts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
    for (auto& x : toasts) {
        if (x.err) ImGui::TextColored(ImVec4(1, 0.7f, 0.76f, 1), "%s", x.text.c_str());
        else ImGui::TextUnformatted(x.text.c_str());
    }
    ImGui::End();
}

void drawAudioMenu() {
    if (!audioMenuOpen) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.3f), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowSize(ImVec2(520 * io.FontGlobalScale, 0));
    ImGui::Begin("Audio input  (arrows + Enter, Esc closes)", &audioMenuOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    int i = 0;
    auto item = [&](const std::string& text, bool active) {
        ImGui::PushID(i);
        if (ImGui::Selectable((std::string(active ? "> " : "   ") + text).c_str(), audioMenuSel == i)) audioMenuActivate(i);
        ImGui::PopID();
        i++;
    };
    for (auto& d : audioDevices) {
        bool active = audio.deviceName() == d.name &&
                      ((d.loopback && audio.source() == AudioEngine::Source::Loopback) || (!d.loopback && audio.source() == AudioEngine::Source::Input));
        item((d.loopback ? "System audio:  " : "Input:  ") + d.name + (d.isDefault ? "  (default)" : ""), active);
    }
    item("Audio file...", audio.source() == AudioEngine::Source::File);
    item("No audio", audio.source() == AudioEngine::Source::None);
    ImGui::End();
}

// ----- group colours shared by the keyboard editor and help
ImU32 groupColor(const std::string& g, int alpha = 255) {
    static const std::map<std::string, ImVec4> c = {
        {"Presets", ImVec4(0.70f, 0.53f, 1.0f, 1)}, {"Cues", ImVec4(1.0f, 0.62f, 0.25f, 1)}, {"Library", ImVec4(0.95f, 0.82f, 0.35f, 1)},
        {"Mixer", ImVec4(0.31f, 0.90f, 0.78f, 1)}, {"Auto-pilot", ImVec4(0.38f, 0.72f, 1.0f, 1)}, {"Tempo", ImVec4(1.0f, 0.43f, 0.78f, 1)},
        {"Performance", ImVec4(1.0f, 0.36f, 0.48f, 1)}, {"FX", ImVec4(0.55f, 0.95f, 0.45f, 1)}, {"Overlays", ImVec4(0.85f, 0.85f, 0.95f, 1)},
        {"Output", ImVec4(0.45f, 0.85f, 1.0f, 1)}, {"Scenes", ImVec4(1.0f, 0.86f, 0.3f, 1)}, {"View", ImVec4(0.62f, 0.60f, 0.70f, 1)}, {"Audio", ImVec4(0.95f, 0.65f, 0.55f, 1)}};
    auto it = c.find(g);
    ImVec4 v = it == c.end() ? ImVec4(0.7f, 0.7f, 0.7f, 1) : it->second;
    v.w = alpha / 255.f;
    return ImGui::ColorConvertFloat4ToU32(v);
}
const char* kGroupOrder[] = {"Presets", "Cues", "Scenes", "Library", "Mixer", "Auto-pilot", "Tempo", "Performance", "FX", "Overlays", "Output", "Audio", "View"};
bool assignable(const Action& a) { return a.kind != Action::Continuous; }  // sliders belong on MIDI knobs

// ----- keyboard editor
struct KeyCap { int key; const char* label; float w; };  // key 0 = gap, -1 = not assignable
const std::vector<std::vector<KeyCap>>& keyboardRows() {
    static const std::vector<std::vector<KeyCap>> rows = {
        {{-1, "Esc", 1}, {0, nullptr, 1}, {SDLK_F1, "F1", 1}, {SDLK_F2, "F2", 1}, {SDLK_F3, "F3", 1}, {SDLK_F4, "F4", 1}, {0, nullptr, .5f},
         {SDLK_F5, "F5", 1}, {SDLK_F6, "F6", 1}, {SDLK_F7, "F7", 1}, {SDLK_F8, "F8", 1}, {0, nullptr, .5f},
         {SDLK_F9, "F9", 1}, {SDLK_F10, "F10", 1}, {SDLK_F11, "F11", 1}, {SDLK_F12, "F12", 1}},
        {{SDLK_BACKQUOTE, "`", 1}, {SDLK_1, "1", 1}, {SDLK_2, "2", 1}, {SDLK_3, "3", 1}, {SDLK_4, "4", 1}, {SDLK_5, "5", 1}, {SDLK_6, "6", 1},
         {SDLK_7, "7", 1}, {SDLK_8, "8", 1}, {SDLK_9, "9", 1}, {SDLK_0, "0", 1}, {SDLK_MINUS, "-", 1}, {SDLK_EQUALS, "=", 1}, {SDLK_BACKSPACE, "Backspace", 2}},
        {{SDLK_TAB, "Tab", 1.5f}, {SDLK_q, "Q", 1}, {SDLK_w, "W", 1}, {SDLK_e, "E", 1}, {SDLK_r, "R", 1}, {SDLK_t, "T", 1}, {SDLK_y, "Y", 1},
         {SDLK_u, "U", 1}, {SDLK_i, "I", 1}, {SDLK_o, "O", 1}, {SDLK_p, "P", 1}, {SDLK_LEFTBRACKET, "[", 1}, {SDLK_RIGHTBRACKET, "]", 1}, {SDLK_BACKSLASH, "\\", 1.5f}},
        {{-1, "Caps", 1.75f}, {SDLK_a, "A", 1}, {SDLK_s, "S", 1}, {SDLK_d, "D", 1}, {SDLK_f, "F", 1}, {SDLK_g, "G", 1}, {SDLK_h, "H", 1},
         {SDLK_j, "J", 1}, {SDLK_k, "K", 1}, {SDLK_l, "L", 1}, {SDLK_SEMICOLON, ";", 1}, {SDLK_QUOTE, "'", 1}, {SDLK_RETURN, "Enter", 2.25f}},
        {{-1, "Shift", 2.25f}, {SDLK_z, "Z", 1}, {SDLK_x, "X", 1}, {SDLK_c, "C", 1}, {SDLK_v, "V", 1}, {SDLK_b, "B", 1}, {SDLK_n, "N", 1},
         {SDLK_m, "M", 1}, {SDLK_COMMA, ",", 1}, {SDLK_PERIOD, ".", 1}, {SDLK_SLASH, "/", 1}, {-1, "Shift", 2.75f}},
        {{-1, "Ctrl", 1.5f}, {-1, "Win", 1.25f}, {-1, "Alt", 1.25f}, {SDLK_SPACE, "Space", 6.25f}, {-1, "Alt", 1.25f}, {-1, "Win", 1.25f},
         {-1, "Menu", 1.25f}, {-1, "Ctrl", 1.5f}},
    };
    return rows;
}
const std::vector<std::vector<KeyCap>>& navRows() {
    static const std::vector<std::vector<KeyCap>> rows = {
        {},
        {{SDLK_INSERT, "Ins", 1}, {SDLK_HOME, "Home", 1}, {SDLK_PAGEUP, "PgUp", 1}},
        {{SDLK_DELETE, "Del", 1}, {SDLK_END, "End", 1}, {SDLK_PAGEDOWN, "PgDn", 1}},
        {},
        {{0, nullptr, 1}, {SDLK_UP, "Up", 1}, {0, nullptr, 1}},
        {{SDLK_LEFT, "Left", 1}, {SDLK_DOWN, "Down", 1}, {SDLK_RIGHT, "Right", 1}},
    };
    return rows;
}

std::string armedAction;  // selected in the list: the next clicked key gets it
int assignKey = 0;        // key whose "assign" popup is open
char keyFilter[64] = "";

void drawKeyCap(const KeyCap& kc, ImVec2 pos, ImVec2 size, int layer) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(kc.key > 0 ? kc.key : (int)(intptr_t)kc.label + (int)pos.x);
    ImGui::InvisibleButton("cap", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImVec2 end(pos.x + size.x, pos.y + size.y);
    if (kc.key < 0) {  // modifiers & Esc
        dl->AddRectFilled(pos, end, IM_COL32(255, 255, 255, 8), 6);
        dl->AddRect(pos, end, IM_COL32(255, 255, 255, 25), 6);
        dl->AddText(ImVec2(pos.x + 6, pos.y + 4), IM_COL32(150, 148, 165, 255), kc.label);
        if (hovered) ImGui::SetTooltip("%s", strcmp(kc.label, "Esc") == 0 ? "Esc is fixed: closes windows, leaves text fields, clears search"
                                                                           : "Modifier: pick the Shift / Ctrl / Alt layer above");
        ImGui::PopID();
        return;
    }
    auto it = keymap.find({kc.key, layer});
    Action* a = it == keymap.end() ? nullptr : actions.find(it->second);
    ImU32 bg = a ? groupColor(a->group, hovered ? 120 : 80) : IM_COL32(255, 255, 255, hovered ? 28 : 14);
    dl->AddRectFilled(pos, end, bg, 6);
    dl->AddRect(pos, end, a ? groupColor(a->group, 200) : IM_COL32(255, 255, 255, 40), 6, 0, (hovered || !armedAction.empty()) ? 2.f : 1.f);
    dl->AddText(ImVec2(pos.x + 6, pos.y + 3), IM_COL32(236, 235, 245, 255), kc.label);
    if (a) {
        float fs = ImGui::GetFontSize() * 0.78f;
        ImVec4 clip(pos.x + 3, pos.y, end.x - 3, end.y - 2);
        dl->AddText(nullptr, fs, ImVec2(pos.x + 5, pos.y + ImGui::GetFontSize() + 2), IM_COL32(255, 255, 255, 235), a->label.c_str(), nullptr,
                    size.x - 8, &clip);
    }
    std::string chord = chordName(kc.key, layer);
    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        ImGui::SetTooltip("%s  ->  %s\nClick: choose action   Right-click: clear   Drag: move to another key", chord.c_str(), a ? a->label.c_str() : "(unassigned)");
    // drag a bound key onto another key to move the shortcut
    if (a && ImGui::BeginDragDropSource()) {
        int payload[2] = {kc.key, layer};
        ImGui::SetDragDropPayload("KEYBIND", payload, sizeof payload);
        ImGui::Text("%s  (%s)", a->label.c_str(), chord.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (auto* p = ImGui::AcceptDragDropPayload("ACTION")) bindKey(kc.key, layer, std::string((const char*)p->Data));
        if (auto* p = ImGui::AcceptDragDropPayload("KEYBIND")) {
            const int* src = (const int*)p->Data;
            auto s = keymap.find({src[0], src[1]});
            if (s != keymap.end() && !(src[0] == kc.key && src[1] == layer)) {
                std::string id = s->second;
                keymap.erase(s);
                bindKey(kc.key, layer, id);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left)) {
        if (!armedAction.empty()) { bindKey(kc.key, layer, armedAction); }
        else { assignKey = kc.key; ImGui::OpenPopup("##assign"); }
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && a) bindKey(kc.key, layer, "");
    if (assignKey == kc.key && ImGui::BeginPopup("##assign")) {
        ImGui::TextColored(kAccent, "%s", chord.c_str());
        static char f[64] = "";
        if (ImGui::IsWindowAppearing()) { f[0] = 0; ImGui::SetKeyboardFocusHere(); }
        ImGui::SetNextItemWidth(260);
        ImGui::InputTextWithHint("##f", "Filter actions...", f, sizeof f);
        if (a && ImGui::Selectable("(clear this key)")) bindKey(kc.key, layer, "");
        ImGui::BeginChild("##al", ImVec2(300, 320));
        std::string lf = lower(f);
        for (const char* g : kGroupOrder) {
            bool header = false;
            for (auto& act : actions.all()) {
                if (act.group != g || !assignable(act) || (!lf.empty() && lower(act.label).find(lf) == std::string::npos)) continue;
                if (!header) { ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(groupColor(g)), "%s", g); header = true; }
                if (ImGui::Selectable(("   " + act.label + "##" + act.id).c_str(), a && a->id == act.id)) {
                    bindKey(kc.key, layer, act.id);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void drawKeyboard() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x - 40, 1380.f * io.FontGlobalScale), 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    static int lastFrame = -10;
    if (ImGui::GetFrameCount() - lastFrame > 1) ImGui::SetNextWindowFocus();  // opens on top
    lastFrame = ImGui::GetFrameCount();
    if (!ImGui::Begin("Keyboard layout", &keyboardOpen, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    static int layer = 0;
    const int layers[] = {0, 1, 2, 4, 3, 6};
    const char* layerNames[] = {"No modifier", "Shift", "Ctrl", "Alt", "Ctrl+Shift", "Ctrl+Alt"};
    for (int i = 0; i < 6; i++) {
        if (i) ImGui::SameLine();
        int count = 0;
        for (auto& [k, id] : keymap) count += k.second == layers[i];
        char lbl[48];
        snprintf(lbl, sizeof lbl, "%s (%d)", layerNames[i], count);
        if (ImGui::RadioButton(lbl, layer == layers[i])) layer = layers[i];
    }
    // holding real modifiers previews that layer
    int held = (io.KeyShift ? 1 : 0) | (io.KeyCtrl ? 2 : 0) | (io.KeyAlt ? 4 : 0);
    int shown = held && !io.WantTextInput ? held : layer;
    ImGui::SameLine(0, 30);
    if (ImGui::Button("Reset to defaults")) ImGui::OpenPopup("##reset");
    if (ImGui::BeginPopup("##reset")) {
        ImGui::Text("Replace all shortcuts with the defaults?");
        if (ImGui::Button("Reset")) { defaultKeymap(); saveSettings(); toast(false, "Shortcuts reset to defaults"); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!captureAction.empty()) {
        Action* ca = actions.find(captureAction);
        ImGui::TextColored(kWarn, "Press the key (with Shift/Ctrl/Alt if you like) for \"%s\" ...  Esc cancels", ca ? ca->label.c_str() : "?");
    } else if (!armedAction.empty()) {
        Action* aa = actions.find(armedAction);
        ImGui::TextColored(kAccent2, "Click keys to assign \"%s\" (layer: %s).  Click the action again to stop.", aa ? aa->label.c_str() : "?",
                           shown ? chordName(0, shown).substr(0, chordName(0, shown).size() - keyName(0).size()).c_str() : "none");
    } else {
        ImGui::TextDisabled("Drag an action onto a key, click a key to choose its action, or double-click an action and press the key. Hold Shift/Ctrl/Alt to see those layers.");
    }

    // left: action list
    float listW = 300 * io.FontGlobalScale;
    ImGui::BeginChild("##actions", ImVec2(listW, 0), ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##kf", "Filter actions...", keyFilter, sizeof keyFilter);
    std::string lf = lower(keyFilter);
    for (const char* g : kGroupOrder) {
        bool header = false;
        for (auto& a : actions.all()) {
            if (a.group != g || !assignable(a) || (!lf.empty() && lower(a.label).find(lf) == std::string::npos)) continue;
            if (!header) { ImGui::SeparatorText(g); header = true; }
            ImGui::PushID(a.id.c_str());
            auto chords = chordsFor(a.id);
            std::string keys;
            for (auto& c : chords) keys += (keys.empty() ? "" : "  ") + c;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + 3), ImVec2(p.x + 4, p.y + ImGui::GetTextLineHeight() - 1), groupColor(g), 2);
            ImGui::SetCursorScreenPos(ImVec2(p.x + 9, p.y));
            if (ImGui::Selectable(a.label.c_str(), armedAction == a.id || captureAction == a.id, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { captureAction = a.id; armedAction.clear(); }
                else armedAction = armedAction == a.id ? "" : a.id;
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ACTION", a.id.c_str(), a.id.size() + 1);
                ImGui::Text("%s", a.label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nDrag onto a key, click then click keys, or double-click and press a key.", keys.empty() ? "no shortcut" : keys.c_str());
            if (!keys.empty()) {
                float w = ImGui::CalcTextSize(keys.c_str()).x;
                ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8, listW - w - 24));
                ImGui::TextDisabled("%s", keys.c_str());
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    // right: the keyboard
    ImGui::SameLine();
    ImGui::BeginChild("##kb", ImVec2(0, 0));
    float avail = ImGui::GetContentRegionAvail().x;
    float unit = std::min(avail / 19.f, 70.f * io.FontGlobalScale);
    float keyH = unit * 1.05f, gap = std::max(3.f, unit * 0.07f);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    auto drawRows = [&](const std::vector<std::vector<KeyCap>>& rows, float x0) {
        for (size_t r = 0; r < rows.size(); r++) {
            float x = x0, y = origin.y + r * (keyH + gap) + (r > 0 ? keyH * 0.35f : 0);
            for (auto& kc : rows[r]) {
                float w = kc.w * unit;
                if (kc.key != 0) drawKeyCap(kc, ImVec2(x, y), ImVec2(w - gap, keyH), shown);
                x += w;
            }
        }
    };
    drawRows(keyboardRows(), origin.x);
    drawRows(navRows(), origin.x + 15.5f * unit);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 6 * (keyH + gap) + keyH * 0.5f));
    // legend
    for (const char* g : kGroupOrder) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + 3), ImVec2(p.x + 12, p.y + 15), groupColor(g), 3);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 16, p.y));
        ImGui::TextDisabled("%s", g);
        ImGui::SameLine(0, 14);
    }
    ImGui::NewLine();
    ImGui::TextDisabled("Keypad keys act like their main-keyboard twins. Sliders (crossfader, speed...) are mapped to MIDI knobs under Control > MIDI.");
    ImGui::EndChild();
    ImGui::End();
}

// ----- help, generated from the live key map
void drawHelp() {
    if (!helpOpen) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x - 40, 1240.f * io.FontGlobalScale), std::min(io.DisplaySize.y - 60, 760.f)), ImGuiCond_Appearing);
    static bool wasOpen = false;
    if (!wasOpen) ImGui::SetNextWindowFocus();  // always opens on top of the panels
    ImGui::Begin("Keyboard shortcuts", &helpOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    wasOpen = helpOpen;
    if (ImGui::Button(("Customize keys..." + keyHint("keyboard")).c_str())) { keyboardOpen = true; }
    ImGui::SameLine();
    ImGui::TextDisabled("This list always matches your current shortcuts.  Esc: close / leave text field / clear search");

    struct Row { std::string keys, label; };
    std::vector<std::pair<std::string, std::vector<Row>>> sections;
    for (const char* g : kGroupOrder) {
        std::vector<Row> rows;
        std::set<std::string> done;
        if (std::string(g) == "Cues") {  // collapse the 1..0 families when they follow the digit pattern
            for (auto fam : {std::make_pair("cue", "Fire cue"), std::make_pair("store_cue", "Store current preset in cue"), std::make_pair("clear_cue", "Clear cue")}) {
                int mods = -1;
                bool pattern = true;
                for (int i = 0; i < 10 && pattern; i++) {
                    std::string id = fam.first + std::to_string((i + 1) % 10);
                    SDL_Keycode want = i == 9 ? SDLK_0 : SDLK_1 + i;
                    bool found = false;
                    for (auto& [k, a] : keymap)
                        if (a == id && k.first == (int)want && (mods < 0 || k.second == mods)) { mods = k.second; found = true; break; }
                    pattern = found;
                }
                if (pattern) {
                    rows.push_back({chordName(SDLK_1, mods) + " ... 0", fam.second});
                    for (int i = 0; i < 10; i++) done.insert(fam.first + std::to_string(i));
                }
            }
        }
        for (auto& a : actions.all()) {
            if (a.group != g || done.count(a.id)) continue;
            auto c = chordsFor(a.id);
            if (c.empty()) continue;
            std::string keys;
            for (auto& x : c) keys += (keys.empty() ? "" : "  /  ") + x;
            rows.push_back({keys, a.label});
        }
        if (!rows.empty()) sections.push_back({g, rows});
    }
    // balance sections over 4 columns
    const int cols = 4;
    size_t total = 0;
    for (auto& s : sections) total += s.second.size() + 2;
    std::vector<std::vector<size_t>> colSecs(cols);
    size_t acc = 0;
    for (size_t i = 0; i < sections.size(); i++) {
        int c = std::min(cols - 1, (int)(acc * cols / std::max<size_t>(1, total)));
        colSecs[c].push_back(i);
        acc += sections[i].second.size() + 2;
    }
    ImGui::BeginChild("##helpbody");
    if (ImGui::BeginTable("helpcols", cols)) {
        for (int c = 0; c < cols; c++) {
            ImGui::TableNextColumn();
            for (size_t si : colSecs[c]) {
                auto& sec = sections[si];
                std::string title = sec.first;
                for (auto& ch : title) ch = (char)toupper((unsigned char)ch);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(groupColor(sec.first)), "%s", title.c_str());
                if (ImGui::BeginTable(sec.first.c_str(), 2, ImGuiTableFlags_SizingFixedFit)) {
                    for (auto& r : sec.second) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(r.keys.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", r.label.c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Every action can also be mapped to MIDI (Control > MIDI), driven by OSC, or used from the phone remote.");
    ImGui::EndChild();
    ImGui::End();
}


void drawScenes() {
    ImGui::SetNextWindowSize(ImVec2(620, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scenes & show", &S.showScenes, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::TextDisabled("A scene is a whole look: both deck presets, crossfader, mix mode, effects and overlays.");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Morph time", &S.sceneMorphBeats, 0, 16, S.sceneMorphBeats < 0.01f ? "instant" : "%.1f beats");
    ImGui::SameLine();
    ImGui::TextDisabled("Click: recall   Shift+click: store   Right-click: rename / clear");
    float w = (ImGui::GetContentRegionAvail().x - 3 * 8) / 4;
    for (int i = 0; i < kScenes; i++) {
        Scene& s = scenes[i];
        ImGui::PushID(i);
        if (i % 4) ImGui::SameLine(0, 8);
        bool active = i == activeScene;
        ImGui::PushStyleColor(ImGuiCol_Button, s.used ? (active ? ImVec4(0.95f, 0.82f, 0.35f, 0.45f) : ImVec4(0.95f, 0.82f, 0.35f, 0.16f)) : ImVec4(1, 1, 1, 0.04f));
        std::string label = (s.used ? s.name : "empty") + "\n" + (i < 8 ? keyHint("scene" + std::to_string(i + 1)) : "") + "##s";
        if (ImGui::Button(label.c_str(), ImVec2(w, 54))) {
            if (ImGui::GetIO().KeyShift) storeScene(i); else recallScene(i);
        }
        ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem()) {
            static char nm[64];
            if (ImGui::IsWindowAppearing()) snprintf(nm, sizeof nm, "%s", s.name.c_str());
            if (ImGui::MenuItem("Store current look here")) storeScene(i);
            ImGui::SetNextItemWidth(180);
            if (s.used && ImGui::InputText("Name", nm, sizeof nm, ImGuiInputTextFlags_EnterReturnsTrue)) { s.name = nm; saveScenes(); }
            if (s.used && ImGui::MenuItem("Clear")) { s = Scene{}; saveScenes(); }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextColored(kAccent, "Show timeline");
    ImGui::SameLine();
    if (ImGui::Button(tlPlaying ? "Stop" : "Play")) timelinePlay(!tlPlaying);
    ImGui::SameLine();
    if (ImGui::Checkbox("Loop", &tlLoop)) saveScenes();
    ImGui::SameLine();
    if (ImGui::Button("+ Step")) { timeline.push_back({std::max(0, activeScene), 4}); saveScenes(); }
    if (tlPlaying && !timeline.empty()) {
        double into = beatNow() - tlStepStart, len = timeline[tlIndex].bars * 4.0;
        ImGui::SameLine();
        ImGui::ProgressBar((float)std::clamp(into / len, 0.0, 1.0), ImVec2(-1, 0), ("step " + std::to_string(tlIndex + 1) + "  bar " + std::to_string((int)(into / 4) + 1)).c_str());
    }
    for (size_t i = 0; i < timeline.size(); i++) {
        ImGui::PushID((int)(1000 + i));
        TlStep& st = timeline[i];
        bool cur = tlPlaying && (int)i == tlIndex;
        ImGui::TextColored(cur ? kAccent2 : kDim, "%2d.", (int)i + 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("##sc", scenes[st.scene].used ? scenes[st.scene].name.c_str() : ("Scene " + std::to_string(st.scene + 1) + " (empty)").c_str())) {
            for (int k = 0; k < kScenes; k++)
                if (ImGui::Selectable(scenes[k].used ? scenes[k].name.c_str() : ("Scene " + std::to_string(k + 1) + " (empty)").c_str(), st.scene == k)) { st.scene = k; saveScenes(); }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputInt("bars", &st.bars)) { st.bars = std::clamp(st.bars, 1, 256); saveScenes(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("^") && i > 0) { std::swap(timeline[i], timeline[i - 1]); saveScenes(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { timeline.erase(timeline.begin() + i); saveScenes(); ImGui::PopID(); break; }
        ImGui::PopID();
    }
    if (timeline.empty()) ImGui::TextDisabled("Add steps to play scenes in order, each for a number of bars, locked to the beat clock.");
    ImGui::End();
}

void drawModulation() {
    ImGui::SetNextWindowSize(ImVec2(640, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Modulation", &S.showMod, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    double beat = beatNow();
    float meters[5] = {bandBass, bandMid, bandTreb, bandLevel, modSource(SrcBeat, beat)};
    for (int i = 0; i < 5; i++) {
        if (i) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::ProgressBar(std::clamp(meters[i], 0.f, 1.f), ImVec2(110, 8), "");
        ImGui::TextDisabled("%s", kModSrcNames[i]);
        ImGui::EndGroup();
    }
    ImGui::TextDisabled("Route audio bands, the beat or tempo-synced LFOs to any effect. Amount can be negative.");
    if (ImGui::Button("+ Add route")) { mods.push_back({}); saveScenes(); }
    ImGui::SameLine();
    if (ImGui::Button("Kick -> zoom pulse")) { ModRoute r; r.src = SrcBass; r.dst = DstZoom; r.amount = 0.25f; r.smooth = 0.3f; mods.push_back(r); saveScenes(); }
    ImGui::SameLine();
    if (ImGui::Button("Hats -> RGB split")) { ModRoute r; r.src = SrcTreble; r.dst = DstRgb; r.amount = 0.6f; r.smooth = 0.2f; mods.push_back(r); saveScenes(); }
    ImGui::SameLine();
    if (ImGui::Button("Slow hue drift")) { ModRoute r; r.src = SrcLfoSaw; r.dst = DstHue; r.amount = 1; r.rate = 64; r.smooth = 0; mods.push_back(r); saveScenes(); }
    bool changed = false;
    if (ImGui::BeginTable("mods", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Target");
        ImGui::TableSetupColumn("Amount");
        ImGui::TableSetupColumn("Rate / smooth");
        ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < mods.size(); i++) {
            ModRoute& r = mods[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); changed |= ImGui::Checkbox("##on", &r.on);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); changed |= ImGui::Combo("##src", &r.src, kModSrcNames, SrcCount);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); changed |= ImGui::Combo("##dst", &r.dst, kModDstNames, DstCount);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); changed |= ImGui::SliderFloat("##amt", &r.amount, -1, 1, "%.2f");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
            if (r.src >= SrcLfoSine) changed |= ImGui::SliderFloat("##rate", &r.rate, 0.25f, 64, "%.2g beats", ImGuiSliderFlags_Logarithmic);
            else changed |= ImGui::SliderFloat("##sm", &r.smooth, 0, 0.95f, "smooth %.2f");
            ImGui::TableNextColumn(); ImGui::ProgressBar(std::clamp(r.state, 0.f, 1.f), ImVec2(-1, 8), "");
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) { mods.erase(mods.begin() + i); saveScenes(); ImGui::PopID(); break; }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    static bool pending = false;
    pending |= changed;
    if (pending && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) { saveScenes(); pending = false; }
    ImGui::End();
}

void drawDjTab() {
    bool on = S.djOn;
    if (ImGui::Checkbox("Enable DJ link (Denon Engine OS / StageLinQ)", &on)) setDjLink(on);
    ImGui::TextWrapped("Connect the PC to the same network as your Engine OS players or mixer (SC LIVE, SC6000, Prime...). "
                       "Amethyst then follows the on-air deck's tempo and beat, and sees track changes.");
    if (!S.djOn) return;
    ImGui::Separator();
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImGui::TextColored(djSnap.connected ? kAccent2 : kWarn, "%s", djSnap.status.empty() ? "starting..." : djSnap.status.c_str());
    if (!djSnap.device.empty()) ImGui::TextDisabled("Device: %s", djSnap.device.c_str());
    int air = StageLinq::onAirDeck(djSnap);
    if (!djSnap.decks.empty() && ImGui::BeginTable("decks", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Deck", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Track");
        ImGui::TableSetupColumn("BPM", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Beat", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Fader", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < djSnap.decks.size(); i++) {
            const DjDeck& d = djSnap.decks[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored((int)i == air ? kAccent2 : kDim, "%d", (int)i + 1);
            ImGui::TableNextColumn(); ImGui::TextUnformatted((d.artist.empty() ? d.title : d.artist + " - " + d.title).c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.1f", d.bpm);
            ImGui::TableNextColumn(); ImGui::Text("%d.%d", (int)std::floor(d.beat / 4) + 1, ((int)std::floor(d.beat) % 4 + 4) % 4 + 1);
            ImGui::TableNextColumn(); ImGui::Text("%.0f%%", d.fader * 100);
            ImGui::TableNextColumn(); ImGui::TextDisabled("%s%s", d.playing ? "playing" : "", (int)i == air ? " ON AIR" : "");
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Checkbox("Use the on-air deck as the tempo/beat clock", &S.djTempo);
    ImGui::SetNextItemWidth(220);
    ImGui::Combo("On track change", &S.djTrackAction, "Do nothing\0Next preset\0Random preset\0Next scene\0");
    ImGui::Checkbox("Show \"Artist - Title\" on screen", &S.djShowTitle);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("##secs", &S.djTitleSecs, 2, 20, "for %.0f s");
    ImGui::TextDisabled("Serato, rekordbox and Traktor: use Ableton Link instead (Control > Tempo source).");
}

void drawLightingTab() {
    ImGui::Checkbox("Send Art-Net (DMX over network)", &S.dmxOn);
    ImGui::TextWrapped("Each fixture takes the colour of one vertical slice of the output, left to right, so lights follow the visuals.");
    static char ip[64];
    if (ImGui::IsWindowAppearing() || strcmp(ip, S.dmxIp.c_str()) != 0) {
        if (!ImGui::IsAnyItemActive()) snprintf(ip, sizeof ip, "%s", S.dmxIp.c_str());
    }
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("Target IP (255.255.255.255 = broadcast)", ip, sizeof ip)) S.dmxIp = ip;
    ImGui::SetNextItemWidth(200); ImGui::InputInt("Universe", &S.dmxUniverse); S.dmxUniverse = std::clamp(S.dmxUniverse, 0, 32767);
    ImGui::SetNextItemWidth(200); ImGui::SliderInt("Fixtures", &S.dmxFixtures, 1, 64);
    ImGui::SetNextItemWidth(200); ImGui::InputInt("Start address", &S.dmxStart); S.dmxStart = std::clamp(S.dmxStart, 1, 512);
    ImGui::SetNextItemWidth(260); ImGui::Combo("Channel layout", &S.dmxMode, kDmxModes, 4);
    ImGui::SetNextItemWidth(200); ImGui::SliderFloat("Master level", &S.dmxLevel, 0, 1);
    int last = S.dmxStart - 1 + S.dmxFixtures * kDmxWidth[std::clamp(S.dmxMode, 0, 3)];
    ImGui::TextDisabled("Uses channels %d-%d.  Packets sent: %d", S.dmxStart, std::min(512, last), artnet.sent);
    if (last > 512) ImGui::TextColored(kWarn, "Too many fixtures for one universe - extra fixtures are skipped.");
    ImVec2 p = ImGui::GetCursorScreenPos();
    float cw = std::min(28.f, (ImGui::GetContentRegionAvail().x - 4) / std::max(1, (int)dmxColors.size()));
    for (size_t i = 0; i < dmxColors.size(); i++)
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x + i * cw, p.y), ImVec2(p.x + (i + 1) * cw - 2, p.y + 24),
                                                  IM_COL32(dmxColors[i][0], dmxColors[i][1], dmxColors[i][2], 255), 3);
    ImGui::Dummy(ImVec2(1, 28));
}

void drawLayerSource() {
    ImGui::TextDisabled("Source:");
    ImGui::SameLine();
    int src = S.layerSource;
    ImGui::RadioButton("Video file", &src, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Camera / capture", &src, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Spout input", &src, 2);
    if (src != S.layerSource) { S.layerSource = src; openLayerSource(); }
    if (S.layerSource == 1) {
        static std::vector<std::string> cams;
        static double lastScan = -10;
        if (now() - lastScan > 5) { cams = VideoPlayer::cameras(); lastScan = now(); }
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Camera", S.cameraIndex < (int)cams.size() ? cams[S.cameraIndex].c_str() : "(none found)")) {
            for (int i = 0; i < (int)cams.size(); i++)
                if (ImGui::Selectable(cams[i].c_str(), S.cameraIndex == i)) { S.cameraIndex = i; openLayerSource(); }
            ImGui::EndCombo();
        }
        if (!video.active() && ImGui::Button("Open camera")) openLayerSource();
    } else if (S.layerSource == 2) {
        auto senders = spoutSenders();
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Spout sender", S.spoutInput.empty() ? "(active sender)" : S.spoutInput.c_str())) {
            if (ImGui::Selectable("(active sender)", S.spoutInput.empty())) S.spoutInput.clear();
            for (auto& s : senders)
                if (ImGui::Selectable(s.c_str(), S.spoutInput == s)) S.spoutInput = s;
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("%s", spoutRxW > 0 ? ("Receiving " + spoutRxConnected + " " + std::to_string(spoutRxW) + "x" + std::to_string(spoutRxH)).c_str()
                                               : (senders.empty() ? "No Spout senders running" : "Waiting for frames..."));
    }
}

void drawUi() {
    ImGuiIO& io = ImGui::GetIO();
    wantThumbs.clear();
    if (defocusSearch) { ImGui::ClearActiveID(); defocusSearch = false; searchHasFocus = false; }
    if (!S.showBrowser || S.uiHidden) searchHasFocus = false;
    if (!S.uiHidden) {
        float top = 0;
        if (S.showMenu) { drawMenuBar(); top = ImGui::GetFrameHeight(); }
        float hudH = 122 * io.FontGlobalScale, cueH = 64 * io.FontGlobalScale;
        float bottom = io.DisplaySize.y - 12;
        float hudY = bottom - hudH;
        float cueY = (S.showHud ? hudY : bottom) - cueH - 8;
        if (S.showHud) drawHud(hudY, hudH);
        if (S.showCues) drawCues(cueY, cueH);
        if (S.showBrowser) drawBrowser(top + 12, (S.showCues ? cueY : S.showHud ? hudY : bottom) - 8);
        if (S.showMixer) drawMixer();
        if (S.showFx) drawFx();
        if (S.showOverlays) drawOverlaysPanel();
        if (S.showOutputs) drawOutputs();
        if (S.showControl) drawControl();
        if (S.showSetlists) drawSetlists();
        if (editorOpen) drawEditor();
        if (keyboardOpen) drawKeyboard();
        if (S.showScenes) drawScenes();
        if (S.showMod) drawModulation();
        drawToasts(top);
    }
    drawHelp();
    drawAudioMenu();
    thumbs.setWanted((S.livePreviews && !S.uiHidden) ? wantThumbs : std::vector<ThumbRenderer::Want>{});
}

// ===================================================================== self-test
std::string selftestDir;

void writePng(const std::string& path, int w, int h, std::vector<unsigned char>& rgba) {
    static uint32_t crcTab[256];
    if (!crcTab[1])
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crcTab[n] = c;
        }
    auto crc = [&](const unsigned char* d, size_t n) {
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < n; i++) c = crcTab[(c ^ d[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    std::vector<unsigned char> raw;
    raw.reserve((size_t)(w * 4 + 1) * h);
    for (int y = h - 1; y >= 0; y--) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + (size_t)y * w * 4, rgba.begin() + (size_t)(y + 1) * w * 4);
    }
    std::vector<unsigned char> z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size(); off += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        z.push_back(off + n == raw.size());
        z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
    }
    uint32_t ad = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) z.push_back((ad >> s) & 0xFF);
    std::ofstream f(fs::u8path(path), std::ios::binary);
    auto be = [&](uint32_t v) { unsigned char c[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v}; f.write((char*)c, 4); };
    auto chunk = [&](const char* type, const std::vector<unsigned char>& d) {
        be((uint32_t)d.size());
        std::vector<unsigned char> td(type, type + 4);
        td.insert(td.end(), d.begin(), d.end());
        f.write((char*)td.data(), td.size());
        be(crc(td.data(), td.size()));
    };
    f.write("\x89PNG\r\n\x1a\n", 8);
    std::vector<unsigned char> ihdr = {(unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8), (unsigned char)w,
                                       (unsigned char)(h >> 24), (unsigned char)(h >> 16), (unsigned char)(h >> 8), (unsigned char)h, 8, 6, 0, 0, 0};
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
}

void captureBackbuffer(const std::string& name, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
    writePng(selftestDir + "\\" + name + ".png", w, h, px);
    fprintf(stderr, "[selftest] %s\n", name.c_str());
}

void pressKey(SDL_Keycode k, Uint16 mod = 0) {
    SDL_KeyboardEvent e{};
    e.type = SDL_KEYDOWN;
    e.windowID = SDL_GetWindowID(win);
    e.keysym.sym = k;
    e.keysym.scancode = SDL_GetScancodeFromKey(k);
    e.keysym.mod = mod;
    handleKey(e, ImGui::GetIO().WantTextInput);
}

struct TestStep { double at; std::function<void()> act; };
std::vector<TestStep> testSteps;
size_t testNext = 0;
double testStart = 0;
std::string pendingShot, pendingOutShot;
std::ofstream testLog;
void testNote(const std::string& s) { testLog << s << std::endl; }
std::string curName(int d) { return decks[d].cur >= 0 ? lib[decks[d].cur].name : "-"; }

UdpProbe artProbe;

// ---- perf test: preset load cost and phone-remote latency (VortexVJ.exe --perftest <dir> --add-folder <presets>)
bool perfMode = false;
std::vector<double> perfFrames;
std::string perfStats(std::vector<double> v) {
    if (v.empty()) return "n=0";
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (double x : v) sum += x;
    int over = (int)std::count_if(v.begin(), v.end(), [](double x) { return x > 40; });
    char b[160];
    snprintf(b, sizeof b, "n=%d avg=%.1f p50=%.1f p95=%.1f max=%.1f ms, >40ms: %d", (int)v.size(), sum / v.size(), v[v.size() / 2],
             v[std::min(v.size() - 1, v.size() * 95 / 100)], v.back(), over);
    return b;
}
std::vector<double> perfLoads, perfHttpA, perfHttpState;
size_t perfMark = 0;
std::atomic<bool> perfHttpDone{false};

void buildPerfTest() {
    double t = 2.5;
    auto at = [&](double dt, std::function<void()> f) { t += dt; testSteps.push_back({t, std::move(f)}); };
    at(0, [] {
        if (getenv("AMETHYST_PERF_NO_ANALYSIS")) { S.analysisOn = false; thumbs.analysisEnabled = false; }
        testNote("presets=" + std::to_string(lib.size()) + " render=" + std::to_string(renderW) + "x" + std::to_string(renderH) + " analysis=" + std::to_string(S.analysisOn));
        perfMark = perfFrames.size();
    });
    at(2.0, [] { testNote("idle frames: " + perfStats(std::vector<double>(perfFrames.begin() + perfMark, perfFrames.end()))); perfMark = perfFrames.size(); });
    for (int i = 0; i < 16; i++)
        at(0.4, [i] {
            int idx = pickRandom(decks[0].cur);
            double t0 = now();
            loadPreset(0, idx, i % 2 == 1);
            perfLoads.push_back((now() - t0) * 1000.0);
        });
    at(0.5, [] {
        testNote("loadPreset (main thread): " + perfStats(perfLoads));
        testNote("frames during clicks: " + perfStats(std::vector<double>(perfFrames.begin() + perfMark, perfFrames.end())));
        S.webOn = true;
        applyNetSettings();
        perfMark = perfFrames.size();
        std::thread([] {
            for (int i = 0; i < 40; i++) {
                perfHttpA.push_back(httpRequestMs(S.webPort, "/a/fx_kaleido", "POST"));
                perfHttpState.push_back(httpRequestMs(S.webPort, "/state"));
                Sleep(40);
            }
            perfHttpDone = true;
        }).detach();
    });
    at(6.0, [] {
        testNote(std::string("remote done=") + (perfHttpDone ? "yes" : "no"));
        testNote("remote /a/fx_kaleido: " + perfStats(perfHttpA));
        testNote("remote /state: " + perfStats(perfHttpState));
        testNote("frames during remote: " + perfStats(std::vector<double>(perfFrames.begin() + perfMark, perfFrames.end())));
    });
    at(0.5, [] { running = false; });
}

void buildSelftest() {
    double t = 2.5;
    auto at = [&](double dt, std::function<void()> f) { t += dt; testSteps.push_back({t, std::move(f)}); };
    at(0, [] { testNote("presets=" + std::to_string(lib.size()) + " A=" + curName(0) + " audio=" + audio.label() + " sources=" + std::to_string(sourceNames.size())); pendingShot = "01-start"; });
    // smart auto-mix: synthetic 128 BPM tracks -> groove 16 bars, breakdown 8 bars, drop 16 bars, silence;
    // the second one has a fakeout: one bar of drop, one bar without bass, then the real drop at beat 104
    auto synth = [](std::function<bool(double)> kickOn, double beats) {
        MusicStructure ms;
        const double rate = 48000, bpm = 128, spb = 60 / bpm, t0 = 1.0;
        const double total = t0 + beats * spb + 3.0;
        std::vector<float> blk(480 * 2);
        std::mt19937 g(7);
        std::uniform_real_distribution<float> nz(-1, 1);
        std::string log;
        for (size_t f0 = 0; f0 < (size_t)(total * rate); f0 += 480) {
            for (int i = 0; i < 480; i++) {
                double tt = (f0 + i) / rate, sb = (tt - t0) / spb;
                float v = 0;
                if (sb >= 0 && sb < beats) {
                    double ph = (sb - std::floor(sb)) * spb;
                    if (kickOn(sb)) v += 0.7f * (float)(std::sin(2 * M_PI * 55 * ph) * std::exp(-ph / 0.12));
                    v += 0.08f * (float)std::sin(2 * M_PI * 440 * tt);                       // pad
                    if (sb >= 80 && sb < 96) v += 0.02f * (float)(sb - 80) * nz(g) * 0.3f;  // build: rising noise
                    else v += 0.03f * nz(g) * (float)std::exp(-std::fmod(ph, spb / 2) / 0.03);  // hats
                }
                blk[i * 2] = blk[i * 2 + 1] = v;
            }
            double tt = f0 / rate;
            ms.feed(blk.data(), 480, tt, (tt - t0) / spb, bpm);
            for (auto& e : ms.take()) {
                char b[96];
                snprintf(b, sizeof b, "%s@%.2f ", MusicStructure::name(e.type), e.beat);
                log += b;
            }
        }
        return log;
    };
    at(0, [synth] {
        testNote("structure: " + synth([](double b) { return b < 64 || b >= 96; }, 160));
        testNote("structure fakeout: " + synth([](double b) { return b < 64 || (b >= 96 && b < 100) || b >= 104; }, 168));
    });
    at(0, [] {  // auto-pilot stays inside the filtered list, even with a stale pre-picked preset
        query = "mashup";
        rebuildView();
        int outside = -1;
        for (int i = 0; i < (int)lib.size() && outside < 0; i++)
            if (std::find(view.begin(), view.end(), i) == view.end()) outside = i;
        upcoming = outside;
        int ok = 0;
        for (int k = 0; k < 10; k++) {
            go(0, 1, false, true);
            int p = decks[0].pendingIdx;
            if (p >= 0 && std::find(view.begin(), view.end(), p) != view.end()) ok++;
            upcoming = outside;
        }
        testNote("auto stays in filter: " + std::to_string(ok) + "/10 (view " + std::to_string(view.size()) + ")");
        query.clear();
        rebuildView();
    });
    // decks + crossfader
    at(1.0, [] { pressKey(SDLK_TAB); pressKey(SDLK_r); testNote("focus=" + std::string(decks[focus].name) + " B=" + curName(1)); S.showMixer = true; });
    at(0.5, [] { actions.invoke("crossfade", true, true, 0.5f); S.mixMode = MixScreen; });
    at(1.5, [] { pendingShot = "02-mixer-screen-50"; });
    at(0.5, [] { pressKey(SDLK_BACKSLASH); testNote("auto crossfade from 0.5"); });
    at(3.0, [] { testNote("xfade after auto=" + std::to_string(S.xfade)); pressKey(SDLK_TAB); S.showMixer = false; S.mixMode = 0; });
    // FX
    at(0.5, [] { pressKey(SDLK_F2); pressKey(SDLK_F4); pressKey(SDLK_F5); S.showFx = true; });
    at(1.5, [] { pendingShot = "03-fx-kaleido-trails-rgb"; });
    at(0.5, [] { actions.invoke("fx_reset", true); S.showFx = false; });
    // overlays: text
    at(0.3, [] { S.text = "AMETHYST"; S.textOn = true; S.textPos = 1; S.textSize = 0.15f; S.showOverlays = true; });
    at(1.0, [] { pendingShot = "04-text-overlay"; });
    at(0.5, [] { S.textOn = false; S.showOverlays = false; });
    // library meta + setlist
    at(0.3, [] {
        pressKey(SDLK_d, KMOD_LCTRL); pressKey(SDLK_4, KMOD_LCTRL); pressKey(SDLK_l, KMOD_LCTRL);
        if (decks[0].cur < 0) { testNote("library empty - skipping meta checks"); return; }
        Meta& m = metaOf(decks[0].cur);
        testNote("meta fav=" + std::to_string(m.fav) + " rating=" + std::to_string(m.rating) + " setlists=" + std::to_string(setlists.size()));
        setPool("fav");
        testNote("fav pool size=" + std::to_string(view.size()));
        setPool("all");
    });
    // tempo
    at(0.3, [] { for (int i = 0; i < 4; i++) { tempo.tap(now() + i * 0.5); } testNote("tap bpm=" + std::to_string(tempo.bpm()) + " src=" + tempo.sourceName()); tempo.setSource(Tempo::Source::Auto); });
    // quantized launch
    at(0.3, [] { S.quantize = 1; S.quantizeManual = true; std::string before = curName(0); pressKey(SDLK_RIGHT); testNote(std::string("launch armed=") + (launch.armed ? "yes" : "no") + " before=" + before); });
    at(1.5, [] { testNote("after quantized launch A=" + curName(0) + " armed=" + (launch.armed ? "yes" : "no")); S.quantizeManual = false; });
    // outputs + mapping + spout + record
    at(0.3, [] { Output* o = openOutput(-1, 0); if (o) { float c[8] = {0.1f, 0.05f, 0.95f, 0.15f, 0.85f, 0.9f, 0.05f, 0.95f}; memcpy(o->c, c, sizeof c); o->grid = true; } S.showOutputs = true; setSpout(true); startRecording(); });
    at(2.0, [] { pendingOutShot = "05-output-cornerpin-grid"; pendingShot = "06-outputs-panel"; });
    at(1.5, [] { testNote(std::string("recording=") + (recorder.active() ? "yes" : "no") + " secs=" + std::to_string(recorder.seconds()) + " dropped=" + std::to_string(recorder.dropped())); stopRecording(); setSpout(false); closeOutput(0); S.showOutputs = false; });
    // network
    at(0.3, [] { S.webOn = true; S.oscOn = true; applyNetSettings(); testNote(std::string("web=") + (web.running() ? "on" : "off") + " osc=" + (osc.running() ? "on" : "off")); S.showControl = true; });
    at(1.0, [] { pendingShot = "07-control-panel"; });
    // editor
    at(0.5, [] { S.showControl = false; openEditor(); testNote("editor chars=" + std::to_string(editorText.size())); });
    at(1.0, [] { pendingShot = "08-editor"; });
    at(0.3, [] {
        MilkDoc d;
        d.parse(editorText);
        bool rt = d.text() == editorText;
        auto eq = d.perFrame();
        eq.push_back("zoom = zoom + 0.08*(bass_att-1);");
        d.setPerFrame(eq);
        d.set("fDecay", 0.9, false);
        MilkDoc d2;
        d2.parse(d.text());
        testNote("visual editor roundtrip=" + std::string(rt ? "ok" : "FAIL") + " perframe=" + std::to_string(d2.perFrame().size()) +
                 "/" + std::to_string(eq.size()) + " decay=" + std::to_string(d2.get("fDecay", -1)));
        editorText = d.text();
        visualEditor.external(editorText);
        applyEditor();
    });
    at(1.0, [] { testNote("visual editor apply: " + editorStatus); pendingShot = "08b-editor-visual"; });
    at(0.5, [] {
        editorOpen = false;
        keyboardOpen = true;
        bindKey(SDLK_y, 0, "blackout");          // remap: Y -> Blackout
        bindKey(SDLK_b, 0, "");                  // and free up B
        pressKey(SDLK_y);
        testNote(std::string("after remap, Y -> blackout=") + (blackoutOn ? "on" : "off"));
        pressKey(SDLK_b);
        testNote(std::string("B unbound, blackout still=") + (blackoutOn ? "on" : "off"));
        pressKey(SDLK_y);
        testNote("help chords for blackout: " + (chordsFor("blackout").empty() ? std::string("-") : chordsFor("blackout")[0]));
    });
    at(1.0, [] { pendingShot = "09-keyboard-editor"; });
    at(0.5, [] { keyboardOpen = false; helpOpen = true; });
    at(1.0, [] { pendingShot = "10-help-live"; });
    at(0.5, [] { helpOpen = false; pressKey(SDLK_u); });
    at(1.0, [] { pendingShot = "11-ui-hidden"; });
    at(0.5, [] {
        pressKey(SDLK_u);
        int broken = 0;
        for (auto& p : lib) broken += p.broken;
        testNote("fps=" + std::to_string((int)ImGui::GetIO().Framerate) + " broken=" + std::to_string(broken) + " render=" + std::to_string(renderW) + "x" + std::to_string(renderH));
        pendingShot = "12-final";
    });
    // ---- DJ link against the built-in fake Engine OS device
    at(0.5, [] {
        static std::atomic<bool> fakeStop{false};
        StageLinq::startFakeDevice(fakeStop);
        S.djTrackAction = 2;
        S.djShowTitle = true;
        setDjLink(true);
    });
    at(4.0, [] {
        testNote("dj status=" + djSnap.status + " | device=" + djSnap.device);
        if (!djSnap.decks.empty()) {
            const DjDeck& d = djSnap.decks[0];
            testNote("dj deck1 title=" + d.title + " artist=" + d.artist + " bpm=" + std::to_string(d.bpm) + " beat=" + std::to_string(d.beat) + " playing=" + std::to_string(d.playing));
        }
        if (djSnap.decks.size() > 1) testNote("dj deck2 title=" + djSnap.decks[1].title + " bpm=" + std::to_string(djSnap.decks[1].bpm) + " playing=" + std::to_string(djSnap.decks[1].playing));
        testNote(std::string("tempo src=") + tempo.sourceName() + " bpm=" + std::to_string(tempo.bpm()));
    });
    at(2.5, [] { testNote("dj last title=" + djLastTitle + " | overlay=" + djTitleText + " | A=" + curName(0)); pendingShot = "13-dj-title"; S.showControl = true; selectDjTab = true; });
    at(0.7, [] { pendingShot = "14-dj-panel"; });
    // ---- scenes, morphing, show timeline
    at(0.5, [] {
        S.showControl = false;
        setDjLink(false);
        tempo.setSource(Tempo::Source::Tap);
        tempo.setBpm(240, now());
        S.fx = FxParams{};
        storeScene(0);
        S.fx.kaleido = true;
        S.fx.hue = 0.5f;
        go(0, 1, true);
        storeScene(1);
        S.sceneMorphBeats = 2;
        recallScene(0);
        testNote("scene recall: morph active=" + std::to_string(morph.active) + " A=" + curName(0));
    });
    at(1.0, [] {
        testNote("after morph: kaleido=" + std::to_string(S.fx.kaleido) + " hue=" + std::to_string(S.fx.hue) + " morph active=" + std::to_string(morph.active));
        timeline = {{0, 1}, {1, 1}};
        tlLoop = true;
        timelinePlay(true);
        S.showScenes = true;
    });
    at(1.6, [] { testNote("timeline step=" + std::to_string(tlIndex) + " active scene=" + std::to_string(activeScene) + " (1 bar at 240 BPM = 1 s)"); pendingShot = "15-scenes"; });
    // ---- modulation
    at(0.5, [] {
        timelinePlay(false);
        S.showScenes = false;
        S.fx = FxParams{};
        mods.clear();
        ModRoute r; r.src = SrcLfoSine; r.dst = DstHue; r.amount = 1; r.rate = 1; r.smooth = 0; mods.push_back(r);
        ModRoute z; z.src = SrcLfoSquare; z.dst = DstZoom; z.amount = 0.5f; z.rate = 2; z.smooth = 0; mods.push_back(z);
        S.showMod = true;
    });
    at(0.8, [] { testNote("mod: hue base=" + std::to_string(S.fx.hue) + " eff=" + std::to_string(modOut.fx.hue) + " | zoom base=" + std::to_string(S.fx.zoom) + " eff=" + std::to_string(modOut.fx.zoom)); pendingShot = "16-modulation"; });
    // ---- transition
    at(0.5, [] { mods.clear(); S.showMod = false; S.transition = TransWipe; S.xfade = 0; go(1, 1, true, true); startXfade(1.f, 2.0); });
    at(1.0, [] { testNote("transition wipe mid xfade=" + std::to_string(S.xfade) + " anim=" + std::to_string(xanim.active) + " from=" + std::to_string(xanim.from) + " to=" + std::to_string(xanim.to) + " elapsed=" + std::to_string(now() - xanim.start) + " dur=" + std::to_string(xanim.dur) + " morph=" + std::to_string(morph.active) + " tl=" + std::to_string(tlPlaying)); pendingShot = "17-transition-wipe"; });
    // ---- Art-Net loopback
    at(1.5, [] {
        bool ok = artProbe.open(6454);
        testNote(std::string("artnet probe bound=") + (ok ? "yes" : "no"));
        S.dmxOn = true; S.dmxIp = "127.0.0.1"; S.dmxFixtures = 8; S.dmxMode = 3; S.dmxStart = 1;
        S.showOutputs = true;
    });
    at(0.6, [] {
        std::vector<unsigned char> pkt, last;
        int count = 0;
        while (artProbe.receive(pkt)) { count++; last = pkt; }
        artProbe.close();
        bool header = last.size() >= 18 && memcmp(last.data(), "Art-Net\0", 8) == 0 && last[8] == 0x00 && last[9] == 0x50;
        int len = last.size() >= 18 ? (last[16] << 8 | last[17]) : 0;
        char rgb[64] = "";
        if (last.size() >= 23) snprintf(rgb, sizeof rgb, " fixture1 dim=%d rgb=%d,%d,%d", last[18], last[19], last[20], last[21]);
        testNote("artnet received " + std::to_string(count) + " packets, header ok=" + std::to_string(header) + " dmx length=" + std::to_string(len) + rgb +
                 " (sent " + std::to_string(artnet.sent) + ")");
        pendingShot = "18-lighting";
    });
    // ---- live inputs
    at(0.4, [] {
        S.dmxOn = false;
        testNote("cameras=" + std::to_string(VideoPlayer::cameras().size()));
        setSpout(true);
        S.layerSource = 2;
        S.spoutInput = "Amethyst";
        S.videoOn = true;
        S.videoOpacity = 0.5f;
        S.showOutputs = false;
        S.showOverlays = true;
    });
    at(1.5, [] { testNote("spout input " + std::to_string(spoutRxW) + "x" + std::to_string(spoutRxH) + " from '" + spoutRxConnected + "'"); pendingShot = "19-spout-input"; });
    // ---- smart library + MIDI template
    at(0.4, [] {
        setSpout(false);
        S.videoOn = false;
        S.showOverlays = false;
        testNote("analysed=" + std::to_string(looks.size()) + " pending=" + std::to_string(thumbs.analysisPending()));
        std::map<std::string, int> colors;
        for (auto& [p, l] : looks) colors[colorOf(l)]++;
        std::string cs;
        for (auto& [c, n] : colors) cs += c + "=" + std::to_string(n) + " ";
        testNote("colours: " + cs);
        if (!looks.empty()) {
            setPool("similar:" + looks.begin()->first);
            testNote("similar pool size=" + std::to_string(view.size()));
            setPool("all");
        }
        applyTemplate(1, "Launchpad X");
        int n = 0;
        for (auto& b : midi.bindings) n += b.device == "Launchpad X";
        testNote("launchpad template bindings=" + std::to_string(n) + " style=" + std::to_string(midi.feedbackStyle["Launchpad X"]));
        S.showBrowser = true;
        setPool("color:Blue");
    });
    at(1.0, [] { pendingShot = "20-colour-pool"; setPool("all"); });
    at(1.0, [] { running = false; });
}

}  // namespace

// ===================================================================== main
// One-time move from the old "Vortex VJ" name: settings in %APPDATA%, presets/textures in Documents.
void migrateLegacy() {
    std::error_code ec;
    fs::path newApp = fs::u8path(plat::appDataDir());
    fs::path oldApp = newApp.parent_path() / "VortexVJ";
    fs::path docs = fs::u8path(plat::documentsDir());
    fs::path oldDocs = docs / "Vortex VJ", newDocs = docs / "Amethyst";
    if (fs::is_directory(oldDocs, ec) && !fs::exists(newDocs, ec)) fs::rename(oldDocs, newDocs, ec);
    if (!fs::is_directory(oldApp, ec) || fs::exists(newApp / "settings.ini", ec)) return;
    for (auto& e : fs::directory_iterator(oldApp, ec)) {
        std::error_code e2;
        if (!e.is_regular_file(e2)) continue;
        std::ifstream in(e.path(), std::ios::binary);
        std::string txt((std::istreambuf_iterator<char>(in)), {});
        auto replaceAll = [&](const std::string& from, const std::string& to) {
            for (size_t p = 0; (p = txt.find(from, p)) != std::string::npos; p += to.size()) txt.replace(p, from.size(), to);
        };
        replaceAll(oldDocs.u8string(), newDocs.u8string());
        replaceAll("\Vortex - ", "\Amethyst - ");
        replaceAll("Vortex VJ", "Amethyst");
        std::ofstream out(newApp / e.path().filename(), std::ios::binary | std::ios::trunc);
        out << txt;
    }
}

int main(int argc, char** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::string extraFolder;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--selftest" && i + 1 < argc) selftestDir = argv[++i];
        else if (a == "--perftest" && i + 1 < argc) { selftestDir = argv[++i]; perfMode = true; }
        else if (a == "--add-folder" && i + 1 < argc) extraFolder = argv[++i];
    }
    if (selftestDir.empty()) migrateLegacy();
    std::string base = selftestDir.empty() ? plat::appDataDir() : selftestDir;
    if (!selftestDir.empty()) {
        fs::create_directories(fs::u8path(selftestDir));
        for (const char* f : {"settings.ini", "library.ini", "layout.ini", "scenes.ini", "analysis.ini"}) { std::error_code ec; fs::remove(fs::u8path(selftestDir + "\\" + f), ec); }
        testLog.open(fs::u8path(selftestDir + "\\selftest.log"), std::ios::trunc);
        S.docsFolderAdded = true;  // sandbox: only the folders we pass in
    }
    gSettingsPath = base + "\\settings.ini";
    gLibraryPath = base + "\\library.ini";
    gImguiIni = base + "\\layout.ini";
    gScenesPath = base + "\\scenes.ini";
    gAnalysisPath = base + "\\analysis.ini";
    loadSettings();
    loadLibrary();
    loadScenes();
    loadLooks();
    if (!extraFolder.empty()) S.folders.push_back(extraFolder);
    if (selftestDir.empty()) {  // the bundled presets folder next to the exe is always in the library
        std::string bundled = (fs::u8path(plat::exeDir()) / "presets").u8string();
        std::error_code ec;
        if (fs::is_directory(fs::u8path(bundled), ec) && std::find(S.folders.begin(), S.folders.end(), bundled) == S.folders.end())
            S.folders.insert(S.folders.begin(), bundled);
    }
    if (!S.docsFolderAdded) {
        std::string docs = (fs::u8path(plat::documentsDir()) / "Amethyst" / "Presets").u8string();
        std::error_code ec;
        if (fs::is_directory(fs::u8path(docs), ec) && std::find(S.folders.begin(), S.folders.end(), docs) == S.folders.end()) S.folders.push_back(docs);
        S.docsFolderAdded = true;
    }

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        MessageBoxA(nullptr, SDL_GetError(), "Amethyst", MB_ICONERROR);
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("Amethyst", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440, 900,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);
    ctx = win ? SDL_GL_CreateContext(win) : nullptr;
    if (!ctx) {
        MessageBoxA(nullptr, "OpenGL 3.3 is required. Please update your graphics driver.", "Amethyst", MB_ICONERROR);
        return 1;
    }
    glewExperimental = GL_TRUE;
    glewInit();
    glGetError();
    thumbWin = SDL_CreateWindow("", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    thumbCtx = SDL_GL_CreateContext(thumbWin);
    SDL_GL_MakeCurrent(win, ctx);

    // Four preset players (two per deck), each with a hidden window + shared GL context and its own thread.
    for (auto& d : decks)
        for (auto& p : d.players) {
            SDL_Window* pw = SDL_CreateWindow("", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
            SDL_GL_MakeCurrent(win, ctx);
            SDL_GLContext pc = pw ? SDL_GL_CreateContext(pw) : nullptr;
            SDL_GL_MakeCurrent(win, ctx);
            if (!pc) {
                MessageBoxA(nullptr, "Could not create the rendering threads (OpenGL context sharing failed).", "Amethyst", MB_ICONERROR);
                return 1;
            }
            playerWindows.push_back(pw);
            playerContexts.push_back(pc);
            p.setMesh(kQuality[S.quality].meshW, kQuality[S.quality].meshH);
            p.start(pw, pc);
        }
    gfx.init();
    if (thumbCtx) {
        thumbs.setFps(S.previewFps);
        thumbs.start(thumbWin, thumbCtx, 192, 108);
    }

    // ImGui: UI context + an overlay context (shares the font atlas) for text overlays
    IMGUI_CHECKVERSION();
    uiCtx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = gImguiIni.c_str();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    float ddpi = 96;
    SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(win), &ddpi, nullptr, nullptr);
    float scale = std::clamp(ddpi / 96.f, 1.f, 3.f);
    {
        static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0100, 0x024F, 0x2000, 0x206F, 0x2190, 0x21FF, 0};
        static const ImWchar symRanges[] = {0x2600, 0x26FF, 0};
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        bool segoe = fs::exists("C:\\Windows\\Fonts\\segoeui.ttf");
        if (segoe) io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17 * scale, &cfg, ranges);
        else io.Fonts->AddFontDefault();
        if (fs::exists("C:\\Windows\\Fonts\\seguisym.ttf")) {
            ImFontConfig m;
            m.MergeMode = true;
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 15 * scale, &m, symRanges);
        }
        if (fs::exists("C:\\Windows\\Fonts\\seguisb.ttf")) fontBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 25 * scale, &cfg, ranges);
        if (fs::exists("C:\\Windows\\Fonts\\segoeuib.ttf")) fontOverlay = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 96, &cfg, ranges);
        if (!fontBig) fontBig = io.Fonts->Fonts[0];
        if (!fontOverlay) fontOverlay = io.Fonts->Fonts[0];
    }
    setupStyle(scale);
    ImGui_ImplSDL2_InitForOpenGL(win, ctx);
    ImGui_ImplOpenGL3_Init("#version 330");
    overlayCtx = ImGui::CreateContext(io.Fonts);
    ImGui::SetCurrentContext(overlayCtx);
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::SetCurrentContext(uiCtx);

    registerActions();
    defaultKeymap();
    if (!savedKeys.empty()) {  // user's map replaces the defaults; unknown actions are dropped
        keymap.clear();
        for (auto& [k, m, id] : savedKeys)
            if (m >= 0 && actions.find(id)) keymap[{k, m}] = id;
    }
    midi.open();
    for (auto& [dev, st] : midi.feedbackStyle)
        if (st >= 2) midi.sendSysex(dev, launchpadSysex(st - 1));
    tempo.setBpm(S.tapBpm, now());
    if (S.link) tempo.enableLink(true);
    tempo.setSource((Tempo::Source)std::clamp(S.tempoSource, 0, 2));
    applyNetSettings();
    if (S.videoOn) openLayerSource();
    if (S.djOn) djLink.start();

    audio.init();
    refreshDevices();
    if (S.audioKind == "file" && !fs::exists(fs::u8path(S.audioDevice))) S.audioKind = "loopback", S.audioDevice.clear();
    startAudioFrom(S.audioKind, S.audioDevice, true);

    rescan();
    for (int d = 0; d < 2; d++) {
        auto it = byPath.find(d ? S.lastB : S.lastA);
        int first = it != byPath.end() ? it->second : (view.empty() ? -1 : (d ? pickRandom(-1) : view[0]));
        if (first >= 0 && !loadPreset(d, first, true)) go(d, 1, true);
    }
    for (auto& so : savedOutputs) {
        if (so.size() < 10) continue;
        int disp = std::atoi(so[0].c_str());
        if (disp >= SDL_GetNumVideoDisplays() || disp == SDL_GetWindowDisplayIndex(win)) continue;
        float c[8];
        for (int k = 0; k < 8; k++) c[k] = (float)std::atof(so[2 + k].c_str());
        openOutput(disp, std::atoi(so[1].c_str()), c);
    }
    if (lib.empty()) toast(false, "Drop .milk presets or a folder onto the window to begin");
    else toast(false, "%d presets  -  %s  -  press H for shortcuts", (int)lib.size(), audio.label().c_str());

    double last = now();
    if (!selftestDir.empty()) { if (perfMode) buildPerfTest(); else buildSelftest(); testStart = now(); }
    SDL_Event e;
    while (running) {
        while (!selftestDir.empty() && testNext < testSteps.size() && now() - testStart >= testSteps[testNext].at) testSteps[testNext++].act();
        const bool textActive = ImGui::GetIO().WantTextInput;
        while (SDL_PollEvent(&e)) {
            bool consumed = false;
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_WINDOWEVENT: {
                    int oi = outputIndexForWindow(e.window.windowID);
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                        if (oi >= 0) { closeOutput(oi); consumed = true; }
                        else if (e.window.windowID == SDL_GetWindowID(win)) running = false;
                    }
                    if (oi >= 0) {
                        if (e.window.event == SDL_WINDOWEVENT_ENTER) SDL_ShowCursor(SDL_DISABLE);
                        if (e.window.event == SDL_WINDOWEVENT_LEAVE) SDL_ShowCursor(SDL_ENABLE);
                        consumed = true;
                    }
                    break;
                }
                case SDL_KEYDOWN: consumed = handleKey(e.key, textActive); break;
                case SDL_KEYUP: handleKeyUp(e.key); break;
                case SDL_MOUSEMOTION:
                    lastMouseMove = now();
                    if (outputIndexForWindow(e.motion.windowID) >= 0) consumed = true;
                    break;
                case SDL_DROPFILE:
                    pendingDrops.push_back(e.drop.file);
                    SDL_free(e.drop.file);
                    consumed = true;
                    break;
                default: break;
            }
            if (!consumed) ImGui_ImplSDL2_ProcessEvent(&e);
        }
        bool anyFocus = SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS;
        for (auto& o : outputs) anyFocus |= (SDL_GetWindowFlags(o.win) & SDL_WINDOW_INPUT_FOCUS) != 0;
        if (!anyFocus && selftestDir.empty()) releaseHeldKeys();
        processDrops();
        processMidi();
        osc.poll(handleRemote);
        web.poll(handleHttp);

        const double t = now();
        const double dt = std::clamp(t - last, 0.0, 0.25);
        if (perfMode) perfFrames.push_back((t - last) * 1000.0);
        last = t;

        pumpAudio();
        tempo.update(t);
        updateDjLink(t);
        updateMorph();
        updateTimeline();
        intakeAnalysis(t);
        {   // background analysis yields whenever the show needs the GPU
            static double slowSince = 0;
            float fps = ImGui::GetIO().Framerate;
            if (fps < 50) slowSince = t;
            // Compiling presets for analysis briefly stalls the GPU driver, so never do it during a show:
            // outputs open, fullscreen, interface hidden, Spout or recording running.
            bool performing = !outputs.empty() || (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) || S.uiHidden || S.spout ||
                              recorder.active();
            thumbs.analysisEnabled = S.analysisOn && !performing && t - slowSince > 3.0;
        }
        updateLeds(t);
        // crossfade animation
        if (xanim.active) {
            float k = (float)std::clamp((t - xanim.start) / xanim.dur, 0.0, 1.0);
            k = k * k * (3 - 2 * k);
            S.xfade = xanim.from + (xanim.to - xanim.from) * k;
            if (k >= 1) xanim.active = false;
        }
        // quantized launch
        if (launch.armed && beatNow() >= launch.dueBeat - 0.02) {
            launch.armed = false;
            if (launch.idx == -2) back(launch.deck, launch.hard);
            else if (launch.random || launch.idx < 0) go(launch.deck, 1, launch.hard, true);
            else if (!loadPreset(launch.deck, launch.idx, launch.hard)) go(launch.deck, 1, true);
        }
        // auto-pilot
        smartTick(t);
        if (S.autoOn && !S.smartMix && !locked && !launch.armed && !xanim.active) {
            int on = S.xfade < 0.5f ? 0 : 1;
            if (t - decks[on].lastSwitch >= S.interval) {
                decks[on].lastSwitch = t;
                if (S.quantize > 0 && !S.autoDecks) requestLaunch(on, -1, 1, false, S.shuffle, true);
                else autoAdvance();
            }
        }
        for (auto& f : thumbs.takeFailures()) {
            auto it = byPath.find(f);
            if (it != byPath.end()) markBroken(it->second, "preview failed to load");
        }
        if (upcoming < 0 || upcoming == decks[0].cur || upcoming == decks[1].cur) upcoming = pickRandom(decks[focus].cur);

        float target = blackoutOn ? 1.f : 0.f;
        blackAmt += std::clamp(target - blackAmt, (float)-dt / 0.8f, (float)dt / 0.8f);
        flashAmt = std::max(0.f, flashAmt - (float)dt / 0.35f);

        SDL_GL_MakeCurrent(win, ctx);
        commitLoads();
        renderFrame(t, dt);

        // control window
        int cw, ch;
        SDL_GL_GetDrawableSize(win, &cw, &ch);
        SDL_GL_SetSwapInterval(outputs.empty() ? 1 : 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, cw, ch);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        {
            float a = renderW / (float)std::max(1, renderH), ca = cw / (float)std::max(1, ch);
            int w = cw, h = ch, x = 0, y = 0;
            if (a > ca) { h = (int)(cw / a); y = (ch - h) / 2; } else { w = (int)(ch * a); x = (cw - w) / 2; }
            gfx.present(finalRT.tex, x, y, w, h, nullptr, false);
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        drawUi();
        if (S.uiHidden && t - lastMouseMove > 2.0) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        ImGui::Render();
        glViewport(0, 0, cw, ch);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!pendingShot.empty()) { captureBackbuffer(pendingShot, cw, ch); pendingShot.clear(); }
        SDL_GL_SwapWindow(win);

        for (size_t i = 0; i < outputs.size(); i++) {
            Output& o = outputs[i];
            SDL_GL_MakeCurrent(o.win, ctx);
            SDL_GL_SetSwapInterval(i == 0 ? 1 : 0);
            int ow, oh;
            SDL_GL_GetDrawableSize(o.win, &ow, &oh);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            gfx.present(outputTexture(o.source), 0, 0, ow, oh, o.c, o.grid);
            if (i == 0 && !pendingOutShot.empty()) { captureBackbuffer(pendingOutShot, ow, oh); pendingOutShot.clear(); }
            SDL_GL_SwapWindow(o.win);
        }
        SDL_GL_MakeCurrent(win, ctx);
    }

    saveScenes();
    if (looksDirty) saveLooks();
    djLink.stop();
    spoutRx.ReleaseReceiver();
    if (tempo.source() == Tempo::Source::External) tempo.setSource(Tempo::Source::Auto);
    S.tempoSource = (int)tempo.source();
    S.tapBpm = (float)tempo.bpm();
    S.link = tempo.linkEnabled();
    saveSettings();
    saveLibrary();
    stopRecording();
    video.close();
    if (spoutNamed) spout.ReleaseSender();
    osc.stop();
    web.stop();
    midi.close();
    thumbs.stop();
    audio.shutdown();
    ImGui::SetCurrentContext(overlayCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext(overlayCtx);
    ImGui::SetCurrentContext(uiCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(uiCtx);
    for (auto& d : decks)
        for (auto& p : d.players) p.stop();
    for (auto c : playerContexts) SDL_GL_DeleteContext(c);
    for (auto w : playerWindows) SDL_DestroyWindow(w);
    for (auto& o : outputs) SDL_DestroyWindow(o.win);
    if (thumbCtx) SDL_GL_DeleteContext(thumbCtx);
    SDL_DestroyWindow(thumbWin);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    CoUninitialize();
    return 0;
}
