// One registry of everything the performer can do. Keyboard, MIDI, OSC and the phone remote
// all invoke actions by id, so every control is available from every surface.
#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

struct Action {
    std::string id;     // stable id used by MIDI maps, OSC paths and the web remote
    std::string label;  // shown in the UI
    std::string group;  // "Presets", "Mixer", "FX", ...
    enum Kind { Trigger, Toggle, Momentary, Continuous } kind = Trigger;
    std::function<void()> trigger;            // Trigger / Toggle
    std::function<void(float)> set;           // Continuous (0..1) / Momentary (1 down, 0 up)
    std::function<float()> get;               // current value for UI/remote (0..1, toggles 0/1)
    bool repeat = false;  // keeps firing while a key is held (steppers like blend +/-)
    bool remote = true;   // shown on the phone remote (UI-only actions such as panel toggles are not)
};

class ActionRegistry {
public:
    void add(Action a) { list_.push_back(std::move(a)); }
    Action* find(const std::string& id) {
        auto it = std::find_if(list_.begin(), list_.end(), [&](const Action& a) { return a.id == id; });
        return it == list_.end() ? nullptr : &*it;
    }
    std::vector<Action>& all() { return list_; }

    // pressed: button down (true) / up (false). value: 0..1 when the source is continuous.
    bool invoke(const std::string& id, bool pressed, bool hasValue = false, float value = 1.f) {
        Action* a = find(id);
        if (!a) return false;
        switch (a->kind) {
            case Action::Continuous:
                if (hasValue && a->set) a->set(std::clamp(value, 0.f, 1.f));
                else if (pressed && a->trigger) a->trigger();
                break;
            case Action::Momentary:
                if (a->set) a->set(pressed ? 1.f : 0.f);
                break;
            default:
                if (pressed && a->trigger) a->trigger();
        }
        return true;
    }

private:
    std::vector<Action> list_;
};
