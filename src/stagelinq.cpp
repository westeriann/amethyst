#include "stagelinq.h"

#include "net.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <random>

namespace {

constexpr int kDiscoveryPort = 51337;

double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------- byte helpers (big-endian)
struct Writer {
    std::vector<unsigned char> b;
    void u8(unsigned v) { b.push_back((unsigned char)v); }
    void u16(unsigned v) { u8(v >> 8); u8(v); }
    void u32(uint32_t v) { u8(v >> 24); u8(v >> 16); u8(v >> 8); u8(v); }
    void u64(uint64_t v) { u32((uint32_t)(v >> 32)); u32((uint32_t)v); }
    void f64(double d) { uint64_t v; memcpy(&v, &d, 8); u64(v); }
    void raw(const void* p, size_t n) { b.insert(b.end(), (const unsigned char*)p, (const unsigned char*)p + n); }
    void str(const std::string& utf8) {  // NetworkString: uint32 byte length + UTF-16BE
        std::vector<unsigned char> s;
        for (size_t i = 0; i < utf8.size();) {
            uint32_t c = (unsigned char)utf8[i];
            int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
            if (extra) c &= (0x3F >> extra);
            for (int k = 1; k <= extra && i + k < utf8.size(); k++) c = (c << 6) | ((unsigned char)utf8[i + k] & 0x3F);
            i += extra + 1;
            if (c >= 0x10000) {
                c -= 0x10000;
                uint32_t hi = 0xD800 + (c >> 10), lo = 0xDC00 + (c & 0x3FF);
                s.push_back(hi >> 8); s.push_back(hi & 0xFF); s.push_back(lo >> 8); s.push_back(lo & 0xFF);
            } else {
                s.push_back(c >> 8); s.push_back(c & 0xFF);
            }
        }
        u32((uint32_t)s.size());
        raw(s.data(), s.size());
    }
};

struct Reader {
    const unsigned char* p;
    size_t n, i = 0;
    bool ok = true;
    bool need(size_t k) { if (i + k > n) ok = false; return ok; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = (uint32_t)p[i] << 24 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 8 | p[i + 3]; i += 4; return v; }
    unsigned u16() { if (!need(2)) return 0; unsigned v = p[i] << 8 | p[i + 1]; i += 2; return v; }
    uint64_t u64() { uint64_t hi = u32(); return hi << 32 | u32(); }
    double f64() { uint64_t v = u64(); double d; memcpy(&d, &v, 8); return d; }
    void raw(void* dst, size_t k) { if (need(k)) { memcpy(dst, p + i, k); i += k; } }
    std::string str() {
        uint32_t len = u32();
        if (!ok || len > 1 << 20 || !need(len)) { ok = false; return {}; }
        std::string out;
        for (size_t k = 0; k + 1 < len; k += 2) {
            uint32_t c = p[i + k] << 8 | p[i + k + 1];
            if (c >= 0xD800 && c < 0xDC00 && k + 3 < len) {
                uint32_t lo = p[i + k + 2] << 8 | p[i + k + 3];
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                k += 2;
            }
            if (c < 0x80) out += (char)c;
            else if (c < 0x800) { out += (char)(0xC0 | c >> 6); out += (char)(0x80 | (c & 0x3F)); }
            else if (c < 0x10000) { out += (char)(0xE0 | c >> 12); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
            else { out += (char)(0xF0 | c >> 18); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
        }
        i += len;
        return out;
    }
};

// ---------------------------------------------------------------- tiny JSON field readers
bool jsonString(const std::string& j, const char* key, std::string& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return false;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return false;
    p = j.find('"', p);
    if (p == std::string::npos) return false;
    out.clear();
    for (size_t i = p + 1; i < j.size(); i++) {
        char c = j[i];
        if (c == '"') return true;
        if (c == '\\' && i + 1 < j.size()) {
            char e = j[++i];
            if (e == 'n') out += '\n';
            else if (e == 't') out += '\t';
            else if (e == 'u' && i + 4 < j.size()) {
                unsigned cp = (unsigned)std::strtoul(j.substr(i + 1, 4).c_str(), nullptr, 16);
                i += 4;
                if (cp < 0x80) out += (char)cp;
                else if (cp < 0x800) { out += (char)(0xC0 | cp >> 6); out += (char)(0x80 | (cp & 0x3F)); }
                else { out += (char)(0xE0 | cp >> 12); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
            } else out += e;
        } else out += c;
    }
    return false;
}
bool jsonNumber(const std::string& j, const char* key, double& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return false;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return false;
    p++;
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) p++;
    if (j.compare(p, 4, "true") == 0) { out = 1; return true; }
    if (j.compare(p, 5, "false") == 0) { out = 0; return true; }
    char* end = nullptr;
    out = std::strtod(j.c_str() + p, &end);
    return end != j.c_str() + p;
}

// ---------------------------------------------------------------- sockets
SOCKET tcpConnect(sockaddr_in addr, double timeout) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return s;
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    connect(s, (sockaddr*)&addr, sizeof addr);
    fd_set w, e;
    FD_ZERO(&w); FD_ZERO(&e);
    FD_SET(s, &w); FD_SET(s, &e);
    timeval tv{(long)timeout, (long)((timeout - (long)timeout) * 1e6)};
    if (select(0, nullptr, &w, &e, &tv) <= 0 || FD_ISSET(s, &e)) { closesocket(s); return INVALID_SOCKET; }
    BOOL nd = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof nd);
    return s;
}
bool sendAll(SOCKET s, const std::vector<unsigned char>& b) {
    size_t off = 0;
    double until = nowSec() + 2;
    while (off < b.size()) {
        int n = send(s, (const char*)b.data() + off, (int)(b.size() - off), 0);
        if (n > 0) off += n;
        else if (WSAGetLastError() == WSAEWOULDBLOCK && nowSec() < until) Sleep(1);
        else return false;
    }
    return true;
}
// Append whatever is readable; false when the peer closed or errored.
bool pump(SOCKET s, std::vector<unsigned char>& buf) {
    char tmp[16384];
    for (;;) {
        int n = recv(s, tmp, sizeof tmp, 0);
        if (n > 0) { buf.insert(buf.end(), tmp, tmp + n); continue; }
        if (n == 0) return false;
        return WSAGetLastError() == WSAEWOULDBLOCK;
    }
}
unsigned localPort(SOCKET s) {
    sockaddr_in a{};
    int len = sizeof a;
    getsockname(s, (sockaddr*)&a, &len);
    return ntohs(a.sin_port);
}

std::vector<unsigned char> discovery(const unsigned char tok[16], const char* action, unsigned port) {
    Writer w;
    w.raw("airD", 4);
    w.raw(tok, 16);
    w.str("Amethyst");
    w.str(action);
    w.str("Amethyst");
    w.str("1.0.0");
    w.u16(port);
    return w.b;
}

std::vector<unsigned char> serviceAnnounce(const unsigned char tok[16], const char* name, unsigned port) {
    Writer w;
    w.u32(0);
    w.raw(tok, 16);
    w.str(name);
    w.u16(port);
    return w.b;
}

}  // namespace

// ==================================================================== client
void StageLinq::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

void StageLinq::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

StageLinq::Snapshot StageLinq::snapshot() {
    std::lock_guard<std::mutex> lk(mu_);
    return snap_;
}

int StageLinq::onAirDeck(const Snapshot& s) {
    int best = -1;
    float bestScore = 0;
    for (size_t i = 0; i < s.decks.size(); i++) {
        const DjDeck& d = s.decks[i];
        if (d.master && d.playing) return (int)i;
        float score = d.playing ? 0.5f + d.fader : 0;
        if (score > bestScore) { bestScore = score; best = (int)i; }
    }
    return best;
}

void StageLinq::run() {
    netInit();
    unsigned char tok[16];
    std::random_device rd;
    for (auto& t : tok) t = (unsigned char)rd();
    tok[0] &= 0x7F;  // reported: some devices ignore tokens with the top bit set

    auto setStatus = [&](const std::string& s, bool connected) {
        std::lock_guard<std::mutex> lk(mu_);
        snap_.status = s;
        snap_.connected = connected;
        if (!connected) snap_.device.clear();
    };

    SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL on = TRUE;
    setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on);
    setsockopt(udp, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof on);
    sockaddr_in la{};
    la.sin_family = AF_INET;
    la.sin_port = htons(kDiscoveryPort);
    la.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(udp, (sockaddr*)&la, sizeof la) != 0) {
        setStatus("UDP port 51337 is busy (is Engine DJ or SoundSwitch running?)", false);
    }
    u_long nb = 1;
    ioctlsocket(udp, FIONBIO, &nb);
    setStatus("Searching for Engine OS devices...", false);

    SOCKET mainS = INVALID_SOCKET, stateS = INVALID_SOCKET, beatS = INVALID_SOCKET;
    std::vector<unsigned char> mainBuf, stateBuf, beatBuf;
    unsigned char devTok[16] = {};
    double lastAnnounce = 0, lastRef = 0, lastData = 0;
    sockaddr_in devAddr{};

    auto disconnect = [&](const std::string& why) {
        for (SOCKET* s : {&mainS, &stateS, &beatS})
            if (*s != INVALID_SOCKET) { closesocket(*s); *s = INVALID_SOCKET; }
        mainBuf.clear(); stateBuf.clear(); beatBuf.clear();
        setStatus(why, false);
    };

    auto deckRef = [&](int i) -> DjDeck& {  // caller holds mu_
        if ((int)snap_.decks.size() <= i) snap_.decks.resize(i + 1);
        return snap_.decks[i];
    };

    auto onState = [&](const std::string& name, const std::string& json) {
        std::lock_guard<std::mutex> lk(mu_);
        int deck = -1;
        std::string rest;
        if (name.rfind("/Engine/Deck", 0) == 0 && name.size() > 13) { deck = name[12] - '1'; rest = name.substr(13); }
        double v = 0;
        std::string s;
        bool hasNum = jsonNumber(json, "value", v) || jsonNumber(json, "state", v);
        if (deck >= 0 && deck < 8) {
            DjDeck& d = deckRef(deck);
            if (rest == "/Track/SongName" && jsonString(json, "string", s)) d.title = s;
            else if (rest == "/Track/ArtistName" && jsonString(json, "string", s)) d.artist = s;
            else if (rest == "/CurrentBPM" && hasNum) d.bpm = v;
            else if ((rest == "/PlayState" || rest == "/Play") && hasNum) d.playing = v > 0.5;
            else if (rest == "/ExternalMixerVolume" && hasNum) d.fader = (float)v;
            else if (rest == "/DeckIsMaster" && hasNum) d.master = v > 0.5;
        } else if (name.rfind("/Mixer/CH", 0) == 0 && name.find("faderPosition") != std::string::npos && hasNum) {
            int ch = name[9] - '1';
            if (ch >= 0 && ch < 8) deckRef(ch).fader = (float)v;
        }
    };

    std::vector<std::string> paths;
    for (int d = 1; d <= 4; d++) {
        std::string p = "/Engine/Deck" + std::to_string(d);
        for (const char* s : {"/Track/SongName", "/Track/ArtistName", "/CurrentBPM", "/PlayState", "/Play", "/ExternalMixerVolume", "/DeckIsMaster"})
            paths.push_back(p + s);
        paths.push_back("/Mixer/CH" + std::to_string(d) + "faderPosition");
    }

    while (running_) {
        const double t = nowSec();
        if (t - lastAnnounce > 1.0) {
            lastAnnounce = t;
            auto pkt = discovery(tok, "DISCOVERER_HOWDY_", 0);
            sockaddr_in b{};
            b.sin_family = AF_INET;
            b.sin_port = htons(kDiscoveryPort);
            b.sin_addr.s_addr = INADDR_BROADCAST;
            sendto(udp, (const char*)pkt.data(), (int)pkt.size(), 0, (sockaddr*)&b, sizeof b);
            inet_pton(AF_INET, "127.0.0.1", &b.sin_addr);
            sendto(udp, (const char*)pkt.data(), (int)pkt.size(), 0, (sockaddr*)&b, sizeof b);
        }
        // discovery
        for (int k = 0; k < 32; k++) {
            unsigned char buf[2048];
            sockaddr_in from{};
            int fl = sizeof from;
            int n = recvfrom(udp, (char*)buf, sizeof buf, 0, (sockaddr*)&from, &fl);
            if (n <= 0) break;
            if (n < 20 || memcmp(buf, "airD", 4) != 0 || memcmp(buf + 4, tok, 16) == 0) continue;
            Reader r{buf, (size_t)n};
            r.i = 20;
            std::string source = r.str(), action = r.str(), software = r.str(), version = r.str();
            unsigned port = r.u16();
            if (!r.ok || action != "DISCOVERER_HOWDY_" || port == 0) continue;
            if (software == "OfflineAnalyzer" || software == "SSS0" || software == "JM08" || software == "Amethyst") continue;
            if (mainS != INVALID_SOCKET) continue;
            devAddr = from;
            devAddr.sin_port = htons((u_short)port);
            memcpy(devTok, buf + 4, 16);
            setStatus("Connecting to " + source + " (" + software + " " + version + ")...", false);
            mainS = tcpConnect(devAddr, 2.0);
            if (mainS == INVALID_SOCKET) { setStatus("Could not connect to " + source, false); continue; }
            Writer req;
            req.u32(2);
            req.raw(tok, 16);
            sendAll(mainS, req.b);
            {
                std::lock_guard<std::mutex> lk(mu_);
                snap_.device = source + " (" + software + " " + version + ")";
            }
            lastData = t;
        }

        if (mainS != INVALID_SOCKET) {
            if (!pump(mainS, mainBuf)) { disconnect("Device disconnected"); continue; }
            // parse main-connection messages
            for (;;) {
                Reader r{mainBuf.data(), mainBuf.size()};
                uint32_t id = r.u32();
                if (!r.ok) break;
                std::string svc;
                unsigned port = 0;
                if (id == 0) { unsigned char tk[16]; r.raw(tk, 16); svc = r.str(); port = r.u16(); }
                else if (id == 1) { r.i += 32; r.u64(); }
                else if (id == 2) { r.i += 16; r.need(0); }
                else { disconnect("Unexpected data from device"); break; }
                if (!r.ok) break;  // incomplete
                mainBuf.erase(mainBuf.begin(), mainBuf.begin() + r.i);
                lastData = t;
                if (id != 0) continue;
                sockaddr_in sa = devAddr;
                sa.sin_port = htons((u_short)port);
                if (svc == "StateMap" && stateS == INVALID_SOCKET) {
                    stateS = tcpConnect(sa, 2.0);
                    if (stateS != INVALID_SOCKET) {
                        sendAll(stateS, serviceAnnounce(tok, "StateMap", localPort(stateS)));
                        for (auto& p : paths) {
                            Writer body;
                            body.raw("smaa", 4);
                            body.u32(0x7D2);
                            body.str(p);
                            body.u32(0);
                            Writer f;
                            f.u32((uint32_t)body.b.size());
                            f.raw(body.b.data(), body.b.size());
                            sendAll(stateS, f.b);
                        }
                    }
                } else if (svc == "BeatInfo" && beatS == INVALID_SOCKET) {
                    beatS = tcpConnect(sa, 2.0);
                    if (beatS != INVALID_SOCKET) {
                        sendAll(beatS, serviceAnnounce(tok, "BeatInfo", localPort(beatS)));
                        Writer start;
                        start.u32(4);
                        start.u32(0);
                        sendAll(beatS, start.b);
                    }
                }
                if (stateS != INVALID_SOCKET || beatS != INVALID_SOCKET) {
                    std::lock_guard<std::mutex> lk(mu_);
                    snap_.connected = true;
                    snap_.status = std::string("Connected") + (stateS != INVALID_SOCKET ? " - track info" : "") + (beatS != INVALID_SOCKET ? " - beat sync" : "");
                }
            }
            if (mainS == INVALID_SOCKET) continue;
            if (t - lastRef > 0.25) {  // keepalive reference
                lastRef = t;
                Writer w;
                w.u32(1);
                w.raw(tok, 16);
                w.raw(devTok, 16);
                w.u64(0);
                if (!sendAll(mainS, w.b)) { disconnect("Device connection lost"); continue; }
            }
        }
        if (stateS != INVALID_SOCKET) {
            if (!pump(stateS, stateBuf)) { closesocket(stateS); stateS = INVALID_SOCKET; }
            for (;;) {
                Reader r{stateBuf.data(), stateBuf.size()};
                uint32_t len = r.u32();
                if (!r.ok || stateBuf.size() < 4 + (size_t)len) break;
                Reader m{stateBuf.data() + 4, len};
                char magic[4];
                m.raw(magic, 4);
                uint32_t type = m.u32();
                if (m.ok && memcmp(magic, "smaa", 4) == 0 && type == 0) {
                    std::string name = m.str(), json = m.str();
                    if (m.ok) onState(name, json);
                }
                stateBuf.erase(stateBuf.begin(), stateBuf.begin() + 4 + len);
                lastData = t;
            }
        }
        if (beatS != INVALID_SOCKET) {
            if (!pump(beatS, beatBuf)) { closesocket(beatS); beatS = INVALID_SOCKET; }
            for (;;) {
                Reader r{beatBuf.data(), beatBuf.size()};
                uint32_t len = r.u32();
                if (!r.ok || beatBuf.size() < 4 + (size_t)len) break;
                Reader m{beatBuf.data() + 4, len};
                uint32_t id = m.u32();
                if (id == 2) {
                    m.u64();  // clock
                    uint32_t n = m.u32();
                    if (m.ok && n <= 8 && 16 + 24ull * n <= len) {
                        std::lock_guard<std::mutex> lk(mu_);
                        for (uint32_t d = 0; d < n; d++) {
                            double beat = m.f64(), total = m.f64(), bpm = m.f64();
                            (void)total;
                            if (!m.ok) break;
                            DjDeck& dk = deckRef((int)d);
                            dk.beat = beat;
                            dk.beatTime = t;
                            dk.hasBeat = true;
                            if (bpm > 0) dk.bpm = bpm;
                        }
                    }
                }
                beatBuf.erase(beatBuf.begin(), beatBuf.begin() + 4 + len);
                lastData = t;
            }
        }
        if (mainS != INVALID_SOCKET && t - lastData > 10) disconnect("Device stopped responding");

        // wait for activity (or 20 ms)
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(udp, &rs);
        for (SOCKET s : {mainS, stateS, beatS})
            if (s != INVALID_SOCKET) FD_SET(s, &rs);
        timeval tv{0, 20000};
        select(0, &rs, nullptr, nullptr, &tv);
    }
    auto bye = discovery(tok, "DISCOVERER_EXIT_", 0);
    sockaddr_in b{};
    b.sin_family = AF_INET;
    b.sin_port = htons(kDiscoveryPort);
    b.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(udp, (const char*)bye.data(), (int)bye.size(), 0, (sockaddr*)&b, sizeof b);
    disconnect("Stopped");
    closesocket(udp);
}

// ==================================================================== fake device (self-test only)
void StageLinq::startFakeDevice(std::atomic<bool>& stop) {
    std::thread([&stop] {
        netInit();
        unsigned char tok[16];
        for (int i = 0; i < 16; i++) tok[i] = (unsigned char)(0x10 + i);
        auto listenOn = [](unsigned& port) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
            a.sin_port = 0;
            bind(s, (sockaddr*)&a, sizeof a);
            listen(s, 4);
            port = localPort(s);
            u_long nb = 1;
            ioctlsocket(s, FIONBIO, &nb);
            return s;
        };
        unsigned pMain, pState, pBeat;
        SOCKET lMain = listenOn(pMain), lState = listenOn(pState), lBeat = listenOn(pBeat);
        SOCKET cMain = INVALID_SOCKET, cState = INVALID_SOCKET, cBeat = INVALID_SOCKET;
        SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        std::vector<unsigned char> mb, sb, bb;
        double start = nowSec(), lastHowdy = 0, lastState = 0, lastBeat = 0;
        bool beatStarted = false, served = false;
        auto acceptOne = [](SOCKET l, SOCKET& c) {
            if (c != INVALID_SOCKET) return;
            SOCKET a = accept(l, nullptr, nullptr);
            if (a != INVALID_SOCKET) { u_long nb = 1; ioctlsocket(a, FIONBIO, &nb); c = a; }
        };
        while (!stop) {
            double t = nowSec();
            if (t - lastHowdy > 0.5) {
                lastHowdy = t;
                Writer w;
                w.raw("airD", 4);
                w.raw(tok, 16);
                w.str("Fake SC LIVE 4");
                w.str("DISCOVERER_HOWDY_");
                w.str("JP14");
                w.str("4.0.0");
                w.u16(pMain);
                sockaddr_in d{};
                d.sin_family = AF_INET;
                d.sin_port = htons(kDiscoveryPort);
                inet_pton(AF_INET, "127.0.0.1", &d.sin_addr);
                sendto(udp, (const char*)w.b.data(), (int)w.b.size(), 0, (sockaddr*)&d, sizeof d);
            }
            acceptOne(lMain, cMain);
            acceptOne(lState, cState);
            acceptOne(lBeat, cBeat);
            if (cMain != INVALID_SOCKET) {
                pump(cMain, mb);
                if (!served && mb.size() >= 20) {  // services request -> list + reference
                    Writer w;
                    auto a = serviceAnnounce(tok, "StateMap", pState);
                    w.raw(a.data(), a.size());
                    a = serviceAnnounce(tok, "BeatInfo", pBeat);
                    w.raw(a.data(), a.size());
                    w.u32(1);
                    w.raw(tok, 16);
                    w.raw(tok, 16);
                    w.u64(0);
                    sendAll(cMain, w.b);
                    served = true;
                }
                mb.clear();
            }
            if (cState != INVALID_SOCKET) {
                pump(cState, sb);
                sb.clear();
                if (t - lastState > 0.5) {
                    lastState = t;
                    bool second = t - start > 4;
                    auto emit = [&](const std::string& name, const std::string& json) {
                        Writer body;
                        body.raw("smaa", 4);
                        body.u32(0);
                        body.str(name);
                        body.str(json);
                        Writer f;
                        f.u32((uint32_t)body.b.size());
                        f.raw(body.b.data(), body.b.size());
                        sendAll(cState, f.b);
                    };
                    emit("/Engine/Deck1/Track/SongName", second ? "{\"string\":\"Second Song\",\"type\":8}" : "{\"string\":\"Test Track \\u00e9\",\"type\":8}");
                    emit("/Engine/Deck1/Track/ArtistName", "{\"string\":\"Fake Artist\",\"type\":8}");
                    emit("/Engine/Deck1/CurrentBPM", "{\"type\":0,\"value\":128.0}");
                    emit("/Engine/Deck1/PlayState", "{\"state\":true,\"type\":1}");
                    emit("/Engine/Deck1/ExternalMixerVolume", "{\"type\":0,\"value\":1.0}");
                    emit("/Engine/Deck2/Track/SongName", "{\"string\":\"Cued Up\",\"type\":8}");
                    emit("/Engine/Deck2/CurrentBPM", "{\"type\":0,\"value\":100.0}");
                    emit("/Engine/Deck2/PlayState", "{\"state\":false,\"type\":1}");
                }
            }
            if (cBeat != INVALID_SOCKET) {
                pump(cBeat, bb);
                for (size_t i = 0; i + 8 <= bb.size(); i++)
                    if (bb[i] == 0 && bb[i + 1] == 0 && bb[i + 2] == 0 && bb[i + 3] == 4 && bb[i + 7] == 0) beatStarted = true;
                if (beatStarted && t - lastBeat > 0.02) {
                    lastBeat = t;
                    Writer body;
                    body.u32(2);
                    body.u64((uint64_t)((t - start) * 1e9));
                    body.u32(2);
                    body.f64((t - start) * 128.0 / 60.0); body.f64(512); body.f64(128);
                    body.f64(0); body.f64(400); body.f64(100);
                    body.f64(0); body.f64(0);
                    Writer f;
                    f.u32((uint32_t)body.b.size());
                    f.raw(body.b.data(), body.b.size());
                    sendAll(cBeat, f.b);
                }
            }
            Sleep(5);
        }
        for (SOCKET s : {lMain, lState, lBeat, cMain, cState, cBeat, udp})
            if (s != INVALID_SOCKET) closesocket(s);
    }).detach();
}
