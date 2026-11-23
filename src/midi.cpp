#include "midi.h"

#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <cstdio>
#include <cstring>

namespace {
struct Ctx {
    Midi* midi;
    int dev;
};
}  // namespace

struct MidiThunk {
    static void push(Midi* m, const MidiMsg& msg) {
        std::lock_guard<std::mutex> lk(m->mu_);
        if (m->queue_.size() < 4096) m->queue_.push_back(msg);
    }
};

static void CALLBACK midiProc(HMIDIIN, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR) {
    if (msg != MIM_DATA) return;
    auto* c = reinterpret_cast<Ctx*>(inst);
    MidiMsg m{c->dev, (unsigned char)(p1 & 0xFF), (unsigned char)((p1 >> 8) & 0x7F), (unsigned char)((p1 >> 16) & 0x7F)};
    if ((m.status & 0xF0) < 0x80 || (m.status & 0xF0) == 0xF0) return;  // ignore clock/sysex
    MidiThunk::push(c->midi, m);
}

void Midi::open() {
    close();
    UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; i++) {
        MIDIINCAPSW caps{};
        midiInGetDevCapsW(i, &caps, sizeof caps);
        names_.push_back(plat::narrow(caps.szPname));
        auto* c = new Ctx{this, (int)i};
        HMIDIIN h = nullptr;
        if (midiInOpen(&h, i, (DWORD_PTR)midiProc, (DWORD_PTR)c, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
            midiInStart(h);
            handles_.push_back(h);
            ctx_.push_back(c);
        } else {
            handles_.push_back(nullptr);
            delete c;
        }
    }
    UINT no = midiOutGetNumDevs();
    for (UINT i = 0; i < no; i++) {
        MIDIOUTCAPSW caps{};
        midiOutGetDevCapsW(i, &caps, sizeof caps);
        outNames_.push_back(plat::narrow(caps.szPname));
        HMIDIOUT h = nullptr;
        outHandles_.push_back(midiOutOpen(&h, i, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR ? (void*)h : nullptr);
    }
}

int Midi::outputFor(const std::string& in) const {
    for (size_t i = 0; i < outNames_.size(); i++)
        if (outNames_[i] == in && outHandles_[i]) return (int)i;
    // Windows often names the pair differently ("MIDIIN2 (X)" / "MIDIOUT2 (X)"): match on the shared part
    auto core = [](std::string s) {
        for (const char* p : {"MIDIIN", "MIDIOUT", "In", "Out"}) {
            size_t k = s.find(p);
            if (k != std::string::npos) s.erase(k, strlen(p));
        }
        return s;
    };
    for (size_t i = 0; i < outNames_.size(); i++)
        if (outHandles_[i] && core(outNames_[i]) == core(in)) return (int)i;
    return -1;
}

void Midi::send(const std::string& dev, unsigned char status, unsigned char d1, unsigned char d2) {
    int o = outputFor(dev);
    if (o < 0) return;
    midiOutShortMsg((HMIDIOUT)outHandles_[o], (DWORD)status | (DWORD)d1 << 8 | (DWORD)d2 << 16);
    ledMessagesSent++;
}

void Midi::sendSysex(const std::string& dev, const std::vector<unsigned char>& bytes) {
    int o = outputFor(dev);
    if (o < 0 || bytes.empty()) return;
    std::vector<char> buf(bytes.begin(), bytes.end());
    MIDIHDR hdr{};
    hdr.lpData = buf.data();
    hdr.dwBufferLength = (DWORD)buf.size();
    HMIDIOUT h = (HMIDIOUT)outHandles_[o];
    if (midiOutPrepareHeader(h, &hdr, sizeof hdr) != MMSYSERR_NOERROR) return;
    midiOutLongMsg(h, &hdr, sizeof hdr);
    for (int i = 0; i < 200 && !(hdr.dwFlags & MHDR_DONE); i++) Sleep(1);
    midiOutUnprepareHeader(h, &hdr, sizeof hdr);
}

void Midi::close() {
    for (void* h : outHandles_)
        if (h) { midiOutReset((HMIDIOUT)h); midiOutClose((HMIDIOUT)h); }
    outHandles_.clear();
    outNames_.clear();
    for (void* h : handles_)
        if (h) {
            midiInStop((HMIDIIN)h);
            midiInClose((HMIDIIN)h);
        }
    handles_.clear();
    for (void* c : ctx_) delete static_cast<Ctx*>(c);  // devices are closed, no more callbacks
    ctx_.clear();
    names_.clear();
}

std::vector<MidiMsg> Midi::poll() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<MidiMsg> out;
    out.swap(queue_);
    if (!out.empty()) {
        const MidiMsg& m = out.back();
        char b[128];
        int type = m.status & 0xF0;
        snprintf(b, sizeof b, "%s  ch%d  %s %d = %d", deviceName(m.device).c_str(), (m.status & 0x0F) + 1,
                 type == 0xB0 ? "CC" : type == 0x90 || type == 0x80 ? "note" : "msg", m.d1, m.d2);
        lastEvent = b;
    }
    return out;
}

std::string Midi::describe(const MidiBinding& b) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s %d  ch%d%s%s", b.type ? "CC" : "Note", b.number, b.channel + 1, b.device.empty() ? "" : "  ",
             b.device.c_str());
    return buf;
}
