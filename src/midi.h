// MIDI input from every connected device (Windows multimedia API) with "MIDI learn".
#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct MidiMsg {
    int device;
    unsigned char status, d1, d2;
};

struct MidiBinding {
    std::string action;
    std::string device;  // device name ("" = any)
    int type = 0;        // 0 = note, 1 = control change
    int channel = 0;     // 0..15
    int number = 0;      // note or CC number
    bool lastHigh = false;  // edge detection for CC buttons
};

class Midi {
public:
    ~Midi() { close(); }
    void open();   // (re)open all input devices
    void close();
    std::vector<MidiMsg> poll();
    const std::vector<std::string>& devices() const { return names_; }
    std::string deviceName(int dev) const { return dev >= 0 && dev < (int)names_.size() ? names_[dev] : ""; }

    std::vector<MidiBinding> bindings;
    std::string learning;   // action id waiting for a control, "" when not learning
    std::string lastEvent;  // human-readable last message (UI)

    static std::string describe(const MidiBinding& b);

    // Output (LED feedback). Outputs are matched to inputs by device name.
    void send(const std::string& inputDevice, unsigned char status, unsigned char d1, unsigned char d2);
    void sendSysex(const std::string& inputDevice, const std::vector<unsigned char>& bytes);
    const std::vector<std::string>& outputs() const { return outNames_; }
    std::map<std::string, int> feedbackStyle;  // device -> 0 generic (127/0), 1 colour palette (Launchpad/APC grid)
    int ledMessagesSent = 0;

private:
    std::vector<void*> handles_;
    std::vector<std::string> names_;
    std::mutex mu_;
    std::vector<MidiMsg> queue_;
    std::vector<void*> ctx_;
    std::vector<void*> outHandles_;
    std::vector<std::string> outNames_;
    int outputFor(const std::string& inputDevice) const;
    friend struct MidiThunk;
};
