#include "preset_editor.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "imgui.h"

// ===================================================================== MilkDoc
static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
static std::string keyOf(const std::string& line) {
    if (line.empty() || line[0] == '[') return {};
    size_t p = line.find('=');
    return p == std::string::npos ? std::string() : lower(trim(line.substr(0, p)));
}
static bool perFrameKey(const std::string& k, int* n) {
    if (k.rfind("per_frame_", 0) != 0 || k.size() <= 10) return false;
    for (size_t i = 10; i < k.size(); i++)
        if (!std::isdigit((unsigned char)k[i])) return false;
    if (n) *n = std::atoi(k.c_str() + 10);
    return true;
}

void MilkDoc::parse(const std::string& text) {
    lines_.clear();
    crlf_ = text.find("\r\n") != std::string::npos;
    trailingNewline_ = !text.empty() && text.back() == '\n';
    size_t a = 0;
    while (a < text.size()) {
        size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        std::string l = text.substr(a, b - a);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines_.push_back(l);
        a = b + 1;
    }
    reindex();
}
std::string MilkDoc::text() const {
    std::string out;
    const char* nl = crlf_ ? "\r\n" : "\n";
    for (size_t i = 0; i < lines_.size(); i++) {
        out += lines_[i];
        if (i + 1 < lines_.size() || trailingNewline_) out += nl;
    }
    return out;
}
void MilkDoc::reindex() {
    idx_.clear();
    for (int i = 0; i < (int)lines_.size(); i++) {
        auto k = keyOf(lines_[i]);
        if (!k.empty()) idx_[k] = i;
    }
}
bool MilkDoc::has(const std::string& key) const { return idx_.count(lower(key)) > 0; }
double MilkDoc::get(const std::string& key, double def) const {
    auto it = idx_.find(lower(key));
    if (it == idx_.end()) return def;
    const auto& l = lines_[it->second];
    const char* s = l.c_str() + l.find('=') + 1;
    char* end = nullptr;
    double v = std::strtod(s, &end);
    return end == s ? def : v;
}
int MilkDoc::insertAt() const {
    for (int i = 0; i < (int)lines_.size(); i++)
        if (lower(lines_[i]).rfind("[preset", 0) == 0) return i + 1;
    return 0;
}
void MilkDoc::set(const std::string& key, double v, bool integer) {
    char buf[64];
    if (integer) snprintf(buf, sizeof buf, "%d", (int)std::lround(v));
    else snprintf(buf, sizeof buf, "%.5f", v);
    auto it = idx_.find(lower(key));
    if (it != idx_.end()) {
        auto& l = lines_[it->second];
        l = l.substr(0, l.find('=') + 1) + buf;
        return;
    }
    if (lines_.empty()) lines_.push_back("[preset00]");
    lines_.insert(lines_.begin() + insertAt(), key + "=" + buf);
    reindex();
}
std::vector<std::string> MilkDoc::perFrame() const {
    std::vector<std::pair<int, std::string>> v;
    for (auto& l : lines_) {
        int n;
        if (perFrameKey(keyOf(l), &n)) v.push_back({n, l.substr(l.find('=') + 1)});
    }
    std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    for (auto& p : v) out.push_back(p.second);
    return out;
}
void MilkDoc::setPerFrame(const std::vector<std::string>& eq) {
    int first = -1;
    for (int i = (int)lines_.size() - 1; i >= 0; i--)
        if (perFrameKey(keyOf(lines_[i]), nullptr)) {
            lines_.erase(lines_.begin() + i);
            first = i;
        }
    if (first < 0) {  // none yet: put them before the per-pixel code, or at the end
        first = (int)lines_.size();
        for (int i = 0; i < (int)lines_.size(); i++)
            if (keyOf(lines_[i]).rfind("per_pixel_", 0) == 0) { first = i; break; }
    }
    for (int i = 0; i < (int)eq.size(); i++)
        lines_.insert(lines_.begin() + first + i, "per_frame_" + std::to_string(i + 1) + "=" + eq[i]);
    reindex();
}

// ===================================================================== editor widgets
static void tipFor(const char* tip, const char* key) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24);
        ImGui::TextUnformatted(tip);
        ImGui::TextDisabled("Right-click to reset.  (%s)", key);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void VisualPresetEditor::after(double def, const char* key, bool integer) {
    if (ImGui::IsItemDeactivatedAfterEdit()) step_ = true;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        doc_.set(key, def, integer);
        changed_ = step_ = true;
    }
}

bool VisualPresetEditor::param(const char* key, const char* label, float lo, float hi, double def, const char* tip, const char* fmt) {
    float v = (float)doc_.get(key, def);
    bool ch = ImGui::SliderFloat(label, &v, lo, hi, fmt);
    if (ch) { doc_.set(key, v, false); changed_ = true; }
    tipFor(tip, key);
    after(def, key, false);
    return ch;
}
bool VisualPresetEditor::intParam(const char* key, const char* label, int lo, int hi, int def, const char* tip) {
    int v = (int)std::lround(doc_.get(key, def));
    bool ch = ImGui::SliderInt(label, &v, lo, hi);
    if (ch) { doc_.set(key, v, true); changed_ = true; }
    tipFor(tip, key);
    after(def, key, true);
    return ch;
}
bool VisualPresetEditor::combo(const char* key, const char* label, const char* const* names, int n, int def, const char* tip) {
    int v = std::clamp((int)std::lround(doc_.get(key, def)), 0, n - 1);
    bool ch = ImGui::Combo(label, &v, names, n);
    if (ch) { doc_.set(key, v, true); changed_ = step_ = true; }
    tipFor(tip, key);
    after(def, key, true);
    return ch;
}
bool VisualPresetEditor::toggle(const char* key, const char* label, bool def, const char* tip) {
    bool v = doc_.get(key, def ? 1 : 0) > 0.5;
    bool ch = ImGui::Checkbox(label, &v);
    if (ch) { doc_.set(key, v ? 1 : 0, true); changed_ = step_ = true; }
    tipFor(tip, key);
    after(def ? 1 : 0, key, true);
    return ch;
}
bool VisualPresetEditor::color(const char* r, const char* g, const char* b, const char* a, const char* label, const float def[4], const char* tip) {
    float c[4] = {(float)doc_.get(r, def[0]), (float)doc_.get(g, def[1]), (float)doc_.get(b, def[2]), a ? (float)doc_.get(a, def[3]) : 1.f};
    bool ch = a ? ImGui::ColorEdit4(label, c, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar)
                : ImGui::ColorEdit3(label, c, ImGuiColorEditFlags_Float);
    if (ch) {
        doc_.set(r, c[0], false), doc_.set(g, c[1], false), doc_.set(b, c[2], false);
        if (a) doc_.set(a, c[3], false);
        changed_ = true;
    }
    tipFor(tip, r);
    if (ImGui::IsItemDeactivatedAfterEdit()) step_ = true;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        doc_.set(r, def[0], false), doc_.set(g, def[1], false), doc_.set(b, def[2], false);
        if (a) doc_.set(a, def[3], false);
        changed_ = step_ = true;
    }
    return ch;
}

// ===================================================================== recipes
struct Recipe {
    const char* name;
    const char* desc;
    std::vector<const char*> lines;
};
static const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> r = {
        {"Pulse with the bass", "The picture zooms in on every kick drum.", {"zoom = zoom + 0.08*(bass_att-1);"}},
        {"Spin with the treble", "Hi-hats and cymbals twist the picture.", {"rot = rot + 0.03*(treb_att-1);"}},
        {"Wobble with the mids", "Vocals and synths make the picture ripple.", {"warp = warp + 0.6*(mid_att-1);"}},
        {"Shake on the beat", "A small camera shake on loud bass hits.",
         {"dx = dx + 0.012*(bass-1)*sin(time*37);", "dy = dy + 0.012*(bass-1)*cos(time*41);"}},
        {"Rainbow waveform", "The waveform slowly cycles through every colour.",
         {"wave_r = 0.5+0.5*sin(time*1.13);", "wave_g = 0.5+0.5*sin(time*1.37);", "wave_b = 0.5+0.5*sin(time*1.71);"}},
        {"Border flashes on kicks", "A bright frame flashes around the edge on the kick.",
         {"ob_size = 0.015;", "ob_a = min(1, max(0, bass-0.9));"}},
        {"Slow breathing", "A gentle in-and-out zoom, even when the music is quiet.", {"zoom = zoom + 0.025*sin(time*0.6);"}},
        {"Drift around", "The centre of motion wanders around the screen.",
         {"cx = 0.5+0.2*sin(time*0.3);", "cy = 0.5+0.2*cos(time*0.23);"}},
        {"Slow spin", "Constant gentle rotation.", {"rot = rot + 0.01;"}},
    };
    return r;
}

void VisualPresetEditor::surprise() {
    static std::mt19937 rng(std::random_device{}());
    auto U = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    doc_.set("zoom", U(0.96f, 1.06f), false);
    doc_.set("rot", U(-0.03f, 0.03f), false);
    doc_.set("warp", U(0.f, 2.f), false);
    doc_.set("fDecay", U(0.92f, 1.f), false);
    doc_.set("nWaveMode", std::uniform_int_distribution<int>(0, 7)(rng), true);
    doc_.set("wave_r", U(0.2f, 1.f), false), doc_.set("wave_g", U(0.2f, 1.f), false), doc_.set("wave_b", U(0.2f, 1.f), false);
    doc_.set("fVideoEchoAlpha", U(0.f, 0.5f), false);
    doc_.set("fVideoEchoZoom", U(0.9f, 1.5f), false);
    doc_.set("nVideoEchoOrientation", std::uniform_int_distribution<int>(0, 3)(rng), true);
    changed_ = step_ = true;
}

// ===================================================================== editor
void VisualPresetEditor::open(const std::string& text) {
    doc_.parse(text);
    original_ = text;
    undo_ = {text};
    redo_.clear();
}
void VisualPresetEditor::external(const std::string& text) {
    doc_.parse(text);
    if (undo_.empty() || undo_.back() != text) {
        undo_.push_back(text);
        if (undo_.size() > 200) undo_.erase(undo_.begin());
        redo_.clear();
    }
}
bool VisualPresetEditor::undo(std::string& text) {
    if (!canUndo()) return false;
    if (undo_.back() != text) undo_.push_back(text);  // keep an unsaved drag
    redo_.push_back(undo_.back());
    undo_.pop_back();
    text = undo_.back();
    doc_.parse(text);
    return true;
}
bool VisualPresetEditor::redo(std::string& text) {
    if (redo_.empty()) return false;
    text = redo_.back();
    redo_.pop_back();
    undo_.push_back(text);
    doc_.parse(text);
    return true;
}
bool VisualPresetEditor::revert(std::string& text) {
    if (text == original_) return false;
    external(original_);
    text = original_;
    return true;
}

bool VisualPresetEditor::draw(std::string& text) {
    changed_ = step_ = false;
    static const float white[4] = {1, 1, 1, 1}, black0[4] = {0, 0, 0, 0}, grey0[4] = {0.25f, 0.25f, 0.25f, 0};
    ImGui::PushItemWidth(-200);

    if (ImGui::BeginTabItem("Make it dance")) {
        ImGui::TextWrapped("Tick a recipe to make this preset react to the music. Each one adds a line to the preset's "
                           "equations (see the Code tab). Untick to remove it again.");
        ImGui::Spacing();
        auto eq = doc_.perFrame();
        for (auto& r : recipes()) {
            bool on = !r.lines.empty();
            for (auto* l : r.lines)
                if (std::find(eq.begin(), eq.end(), std::string(l)) == eq.end()) on = false;
            if (ImGui::Checkbox(r.name, &on)) {
                for (auto* l : r.lines) eq.erase(std::remove(eq.begin(), eq.end(), std::string(l)), eq.end());
                if (on)
                    for (auto* l : r.lines) eq.push_back(l);
                doc_.setPerFrame(eq);
                changed_ = step_ = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", r.desc);
        }
        ImGui::Separator();
        if (ImGui::Button("Surprise me")) surprise();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Randomise motion, trails, waveform and echo.");
        ImGui::SameLine();
        ImGui::TextDisabled("%d equation line%s in this preset", (int)eq.size(), eq.size() == 1 ? "" : "s");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Motion")) {
        ImGui::TextDisabled("How the picture moves every frame. Small changes go a long way.");
        param("zoom", "Zoom", 0.8f, 1.2f, 1.0, "Above 1 flies into the picture, below 1 pulls away.");
        param("fZoomExponent", "Zoom curve", 0.25f, 4.f, 1.0, "How much stronger the zoom is at the edges than the centre.");
        param("rot", "Spin", -0.2f, 0.2f, 0.0, "Rotation per frame. Negative spins the other way.");
        param("warp", "Warp amount", 0.f, 5.f, 1.0, "Wavy, liquid distortion.");
        param("fWarpAnimSpeed", "Warp speed", 0.f, 5.f, 1.0, "How fast the warp ripples move.");
        param("fWarpScale", "Warp size", 0.1f, 5.f, 1.0, "Bigger values give larger, smoother ripples.");
        ImGui::SeparatorText("Centre and drift");
        param("cx", "Centre X", 0.f, 1.f, 0.5, "Horizontal point that zoom and spin happen around.");
        param("cy", "Centre Y", 0.f, 1.f, 0.5, "Vertical point that zoom and spin happen around.");
        param("dx", "Drift sideways", -0.05f, 0.05f, 0.0, "Slides the picture left or right every frame.");
        param("dy", "Drift up/down", -0.05f, 0.05f, 0.0, "Slides the picture up or down every frame.");
        param("sx", "Stretch X", 0.8f, 1.2f, 1.0, "Stretches or squeezes horizontally.");
        param("sy", "Stretch Y", 0.8f, 1.2f, 1.0, "Stretches or squeezes vertically.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Trails & look")) {
        param("fDecay", "Trail length", 0.8f, 1.f, 0.98, "How long old frames linger. 1 = endless trails, lower = quick fade.");
        param("fGammaAdj", "Brightness", 1.f, 4.f, 2.0, "Overall brightness boost.");
        ImGui::SeparatorText("Echo (a second copy of the picture on top)");
        param("fVideoEchoAlpha", "Echo strength", 0.f, 1.f, 0.0, "0 hides the echo, 1 shows it fully.");
        param("fVideoEchoZoom", "Echo zoom", 0.25f, 4.f, 2.0, "Size of the echo copy.");
        static const char* orient[] = {"Normal", "Mirror left-right", "Mirror top-bottom", "Mirror both"};
        combo("nVideoEchoOrientation", "Echo mirror", orient, 4, 0, "Flips the echo copy for kaleidoscope-like symmetry.");
        ImGui::SeparatorText("Filters");
        toggle("bBrighten", "Brighten", false, "Lifts dark areas.");
        ImGui::SameLine(200);
        toggle("bDarken", "Darken", false, "Pushes the picture darker.");
        toggle("bSolarize", "Solarize", false, "Psychedelic colour inversion of bright areas.");
        ImGui::SameLine(200);
        toggle("bInvert", "Invert colours", false, "Photo-negative look.");
        toggle("bDarkenCenter", "Dark centre", false, "Darkens the middle so it doesn't burn out to white.");
        ImGui::SameLine(200);
        toggle("bTexWrap", "Wrap edges", true, "Things leaving one edge come back on the other side.");
        param("fShader", "Colour shimmer", 0.f, 1.f, 0.0, "Adds a hue-shifting shimmer to the trails.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Waveform")) {
        ImGui::TextDisabled("The line or shape drawn from the sound itself.");
        static const char* modes[] = {"Circle", "Spiral scope", "Dot spiral", "Loud spiral", "Wavy line", "Starburst", "Straight line", "Double line"};
        combo("nWaveMode", "Shape", modes, 8, 0, "Which kind of waveform is drawn.");
        color("wave_r", "wave_g", "wave_b", "fWaveAlpha", "Colour", white, "Waveform colour and see-through-ness.");
        param("fWaveScale", "Size", 0.1f, 4.f, 1.0, "How big the waveform reacts to the volume.");
        param("fWaveSmoothing", "Smoothness", 0.f, 0.95f, 0.75, "Higher values give a smoother, calmer line.");
        param("fWaveParam", "Shape tweak", -1.f, 1.f, 0.0, "Changes the shape in a mode-specific way.");
        param("wave_x", "Position X", 0.f, 1.f, 0.5, "Horizontal position.");
        param("wave_y", "Position Y", 0.f, 1.f, 0.5, "Vertical position.");
        toggle("bAdditiveWaves", "Glow (additive)", false, "Overlapping parts add up and glow.");
        ImGui::SameLine(200);
        toggle("bWaveDots", "Dots", false, "Draw dots instead of a line.");
        toggle("bWaveThick", "Thick", false, "Draw a thicker line.");
        ImGui::SameLine(200);
        toggle("bMaximizeWaveColor", "Vivid colour", true, "Push the colour to full brightness.");
        toggle("bModWaveAlphaByVolume", "Fade when quiet", false, "Quiet music makes the waveform fainter.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Borders & grid")) {
        ImGui::SeparatorText("Outer border");
        param("ob_size", "Thickness##ob", 0.f, 0.5f, 0.01, "Width of the frame drawn around the edge.");
        color("ob_r", "ob_g", "ob_b", "ob_a", "Colour##ob", black0, "Set the alpha (A) above 0 to see the border.");
        ImGui::SeparatorText("Inner border");
        param("ib_size", "Thickness##ib", 0.f, 0.5f, 0.01, "Width of the inner frame.");
        color("ib_r", "ib_g", "ib_b", "ib_a", "Colour##ib", grey0, "Set the alpha (A) above 0 to see the border.");
        ImGui::SeparatorText("Motion vectors (a grid of little lines showing movement)");
        param("nMotionVectorsX", "Columns", 0.f, 64.f, 12, "How many columns of lines.", "%.0f");
        param("nMotionVectorsY", "Rows", 0.f, 48.f, 9, "How many rows of lines.", "%.0f");
        param("mv_l", "Line length", 0.f, 5.f, 0.9, "Length of each line.");
        param("mv_dx", "Offset X", -1.f, 1.f, 0.0, "Shifts the grid sideways.");
        param("mv_dy", "Offset Y", -1.f, 1.f, 0.0, "Shifts the grid up or down.");
        static const float mvd[4] = {1, 1, 1, 0};
        color("mv_r", "mv_g", "mv_b", "mv_a", "Colour##mv", mvd, "Set the alpha (A) above 0 to see the grid.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Shapes")) {
        ImGui::TextDisabled("Up to four shapes (circles, triangles, polygons) floating on top.");
        ImGui::SliderInt("Shape", &shape_, 0, 3, "Shape %d");
        char k[16][48];
        const char* names[] = {"enabled", "sides", "additive", "thickOutline", "textured", "x", "y", "rad", "ang", "r", "g", "b", "a", "border_r", "border_g", "border_b"};
        for (int i = 0; i < 16; i++) snprintf(k[i], sizeof k[i], "shapecode_%d_%s", shape_, names[i]);
        char r2[4][48];
        const char* n2[] = {"r2", "g2", "b2", "a2"};
        for (int i = 0; i < 4; i++) snprintf(r2[i], sizeof r2[i], "shapecode_%d_%s", shape_, n2[i]);
        char ba[48];
        snprintf(ba, sizeof ba, "shapecode_%d_border_a", shape_);
        toggle(k[0], "Show this shape", false, "Turn the shape on or off.");
        intParam(k[1], "Corners", 3, 100, 4, "3 = triangle, 4 = square, lots = circle.");
        param(k[5], "Position X##s", 0.f, 1.f, 0.5, "Horizontal position.");
        param(k[6], "Position Y##s", 0.f, 1.f, 0.5, "Vertical position.");
        param(k[7], "Size##s", 0.f, 1.f, 0.1, "Radius of the shape.");
        param(k[8], "Angle##s", 0.f, 6.2832f, 0.0, "Rotation of the shape (radians).");
        static const float sc[4] = {1, 0, 0, 1}, sc2[4] = {0, 1, 0, 0}, bc[4] = {1, 1, 1, 0.1f};
        color(k[9], k[10], k[11], k[12], "Centre colour", sc, "Colour in the middle of the shape.");
        color(r2[0], r2[1], r2[2], r2[3], "Edge colour", sc2, "Colour at the edges; blends into the centre colour.");
        color(k[13], k[14], k[15], ba, "Outline", bc, "Outline colour; alpha 0 hides it.");
        toggle(k[2], "Glow (additive)", false, "The shape adds light instead of covering what's below.");
        ImGui::SameLine(200);
        toggle(k[3], "Thick outline", false, "Draw a thicker outline.");
        toggle(k[4], "Show the picture inside", false, "Fill the shape with a copy of the visual.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Extra waves")) {
        ImGui::TextDisabled("Up to four additional waveforms. Their exact shape comes from equations (Code tab).");
        ImGui::SliderInt("Wave", &wave_, 0, 3, "Wave %d");
        char k[12][48];
        const char* names[] = {"enabled", "samples", "sep", "bSpectrum", "bUseDots", "bDrawThick", "bAdditive", "scaling", "smoothing", "r", "g", "b"};
        for (int i = 0; i < 12; i++) snprintf(k[i], sizeof k[i], "wavecode_%d_%s", wave_, names[i]);
        char ka[48];
        snprintf(ka, sizeof ka, "wavecode_%d_a", wave_);
        toggle(k[0], "Show this wave", false, "Turn the extra waveform on or off.");
        intParam(k[1], "Detail", 2, 512, 512, "How many points the wave is made of.");
        intParam(k[2], "Stereo gap", 0, 256, 0, "Separation between left and right channels.");
        param(k[7], "Size##w", 0.f, 5.f, 1.0, "How strongly the wave reacts.");
        param(k[8], "Smoothness##w", 0.f, 1.f, 0.5, "Higher values make it calmer.");
        color(k[9], k[10], k[11], ka, "Colour##w", white, "Wave colour.");
        toggle(k[3], "Use frequencies (spectrum)", false, "Show bass-to-treble instead of the raw sound wave.");
        toggle(k[4], "Dots##w", false, "Draw dots instead of a line.");
        ImGui::SameLine(200);
        toggle(k[5], "Thick##w", false, "Thicker line.");
        toggle(k[6], "Glow (additive)##w", false, "Overlaps add up and glow.");
        ImGui::EndTabItem();
    }

    ImGui::PopItemWidth();
    if (changed_) text = doc_.text();
    if (step_ && (undo_.empty() || undo_.back() != text)) {
        undo_.push_back(text);
        if (undo_.size() > 200) undo_.erase(undo_.begin());
        redo_.clear();
    }
    return changed_;
}
