// Newbie-friendly visual editor for MilkDrop .milk presets: sliders, switches and colour pickers with
// plain-English names, one-click "make it dance" recipes, undo/redo. Unknown lines are kept verbatim so
// a preset round-trips unchanged.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class MilkDoc {
public:
    void parse(const std::string& text);
    std::string text() const;
    bool has(const std::string& key) const;
    double get(const std::string& key, double def) const;
    void set(const std::string& key, double v, bool integer);
    // per_frame_N equation lines (the part after '=')
    std::vector<std::string> perFrame() const;
    void setPerFrame(const std::vector<std::string>& lines);

private:
    std::vector<std::string> lines_;
    std::unordered_map<std::string, int> idx_;  // lower-case key -> line
    bool crlf_ = false, trailingNewline_ = true;
    int insertAt() const;
    void reindex();
};

class VisualPresetEditor {
public:
    // Load a preset's text (resets undo history).
    void open(const std::string& text);
    // Draw the visual tab items; call between ImGui::BeginTabBar/EndTabBar. Returns true when `text` changed.
    bool draw(std::string& text);
    bool undo(std::string& text);
    bool redo(std::string& text);
    bool revert(std::string& text);
    bool canUndo() const { return undo_.size() > 1; }
    bool canRedo() const { return !redo_.empty(); }
    // Call after the text was changed elsewhere (code tab, reload): re-parses and records an undo step.
    void external(const std::string& text);

private:
    MilkDoc doc_;
    std::string original_;
    std::vector<std::string> undo_, redo_;
    int shape_ = 0, wave_ = 0;
    bool changed_ = false, step_ = false;
    void after(double def, const char* key, bool integer);  // right-click reset + undo bookkeeping
    bool param(const char* key, const char* label, float lo, float hi, double def, const char* tip, const char* fmt = "%.3f");
    bool intParam(const char* key, const char* label, int lo, int hi, int def, const char* tip);
    bool combo(const char* key, const char* label, const char* const* names, int n, int def, const char* tip);
    bool toggle(const char* key, const char* label, bool def, const char* tip);
    bool color(const char* r, const char* g, const char* b, const char* a, const char* label, const float def[4], const char* tip);
    void surprise();
};
