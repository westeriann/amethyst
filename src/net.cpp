#include "net.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {
double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
void setNonBlocking(SOCKET s) {
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
}
uint32_t be32(const unsigned char* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
size_t pad4(size_t n) { return (n + 3) & ~(size_t)3; }
}  // namespace

bool netInit() {
    static bool ok = [] {
        WSADATA w;
        return WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }();
    return ok;
}

// ------------------------------------------------------------------ OSC
bool OscServer::start(int port, std::string& err) {
    stop();
    if (!netInit()) { err = "networking unavailable"; return false; }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { err = "could not create socket"; return false; }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) {
        closesocket(s);
        err = "port " + std::to_string(port) + " is in use";
        return false;
    }
    setNonBlocking(s);
    sock_ = s;
    port_ = port;
    return true;
}

void OscServer::stop() {
    if (running()) closesocket((SOCKET)sock_);
    sock_ = ~0ull;
}

static void parseOsc(const unsigned char* d, size_t n, const std::function<void(const std::string&, bool, float)>& cb, std::string& last) {
    if (n < 4) return;
    if (n >= 16 && memcmp(d, "#bundle", 8) == 0) {  // bundle: time tag then [size, element]*
        size_t p = 16;
        while (p + 4 <= n) {
            uint32_t sz = be32(d + p);
            p += 4;
            if (p + sz > n) break;
            parseOsc(d + p, sz, cb, last);
            p += sz;
        }
        return;
    }
    size_t alen = strnlen((const char*)d, n);
    std::string addr((const char*)d, alen);
    size_t p = pad4(alen + 1);
    bool has = false;
    float v = 1.f;
    if (p < n && d[p] == ',') {
        size_t tlen = strnlen((const char*)d + p, n - p);
        std::string tags((const char*)d + p + 1, tlen - 1);
        size_t q = pad4(p + tlen + 1);
        if (!tags.empty() && q + 4 <= n) {
            uint32_t raw = be32(d + q);
            if (tags[0] == 'f') { memcpy(&v, &raw, 4); has = true; }
            else if (tags[0] == 'i') { v = (float)(int32_t)raw; has = true; }
            else if (tags[0] == 'T') { v = 1; has = true; }
            else if (tags[0] == 'F') { v = 0; has = true; }
        }
    }
    last = addr + (has ? " " + std::to_string(v) : "");
    cb(addr, has, v);
}

void OscServer::poll(const std::function<void(const std::string&, bool, float)>& cb) {
    if (!running()) return;
    unsigned char buf[4096];
    for (int i = 0; i < 256; i++) {
        int n = recv((SOCKET)sock_, (char*)buf, sizeof buf, 0);
        if (n <= 0) break;
        parseOsc(buf, (size_t)n, cb, lastMessage);
    }
}

// ------------------------------------------------------------------ HTTP
bool HttpServer::start(int port, std::string& err) {
    stop();
    if (!netInit()) { err = "networking unavailable"; return false; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { err = "could not create socket"; return false; }
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&reuse, sizeof reuse);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0 || listen(s, 16) != 0) {
        closesocket(s);
        err = "port " + std::to_string(port) + " is in use";
        return false;
    }
    setNonBlocking(s);
    sock_ = s;
    port_ = port;
    return true;
}

void HttpServer::stop() {
    for (auto& c : clients_) closesocket((SOCKET)c.sock);
    clients_.clear();
    if (running()) closesocket((SOCKET)sock_);
    sock_ = ~0ull;
}

void HttpServer::poll(const std::function<HttpResponse(const std::string&, const std::string&, const std::string&)>& handler) {
    if (!running()) return;
    for (int i = 0; i < 16; i++) {
        SOCKET c = accept((SOCKET)sock_, nullptr, nullptr);
        if (c == INVALID_SOCKET) break;
        setNonBlocking(c);
        BOOL nd = TRUE;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof nd);
        clients_.push_back({(unsigned long long)c, {}, nowSec()});
    }
    const double t = nowSec();
    for (size_t i = 0; i < clients_.size();) {
        Client& cl = clients_[i];
        bool dead = false;
        for (;;) {  // drain everything that arrived
            char buf[8192];
            int n = recv((SOCKET)cl.sock, buf, sizeof buf, 0);
            if (n > 0) { cl.buf.append(buf, n); cl.started = t; continue; }
            if (n == 0 || WSAGetLastError() != WSAEWOULDBLOCK) dead = true;
            break;
        }
        // Keep-alive: answer every complete request on this connection, in order.
        for (;;) {
            size_t end = cl.buf.find("\r\n\r\n");
            if (end == std::string::npos) break;
            std::string head = cl.buf.substr(0, end);
            size_t bodyLen = 0;
            std::string lower = head;
            for (auto& ch : lower) ch = (char)tolower((unsigned char)ch);
            size_t cl_ = lower.find("content-length:");
            if (cl_ != std::string::npos) bodyLen = (size_t)std::strtoul(lower.c_str() + cl_ + 15, nullptr, 10);
            if (cl.buf.size() < end + 4 + bodyLen) break;  // body still arriving
            bool closeAfter = lower.find("connection: close") != std::string::npos;
            cl.buf.erase(0, end + 4 + bodyLen);
            std::string line = head.substr(0, head.find("\r\n"));
            std::string method = line.substr(0, line.find(' '));
            size_t p1 = line.find(' '), p2 = line.rfind(' ');
            std::string target = (p1 != std::string::npos && p2 > p1) ? line.substr(p1 + 1, p2 - p1 - 1) : "/";
            std::string path = target.substr(0, target.find('?'));
            std::string query = target.find('?') == std::string::npos ? "" : target.substr(target.find('?') + 1);
            HttpResponse r = handler(method, path, query);
            std::string out = "HTTP/1.1 " + std::to_string(r.status) + (r.status == 200 ? " OK" : " Not Found") +
                              "\r\nContent-Type: " + r.contentType + "\r\nContent-Length: " + std::to_string(r.body.size()) +
                              "\r\nCache-Control: no-store\r\nConnection: " + (closeAfter ? "close" : "keep-alive") + "\r\n\r\n" + r.body;
            size_t off = 0;
            double until = nowSec() + 1.0;
            while (off < out.size()) {  // responses are small; the socket buffer takes them at once
                int n = send((SOCKET)cl.sock, out.data() + off, (int)(out.size() - off), 0);
                if (n > 0) off += n;
                else if (WSAGetLastError() == WSAEWOULDBLOCK && nowSec() < until) Sleep(0);
                else { dead = true; break; }
            }
            if (closeAfter) { shutdown((SOCKET)cl.sock, SD_SEND); dead = true; break; }
        }
        if (t - cl.started > 15 || cl.buf.size() > 65536) dead = true;  // idle keep-alive connection
        if (dead) {
            closesocket((SOCKET)cl.sock);
            clients_.erase(clients_.begin() + i);
        } else {
            i++;
        }
    }
}

std::vector<std::string> HttpServer::localAddresses() {
    std::vector<std::string> out;
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buf(size);
    auto* aa = (IP_ADAPTER_ADDRESSES*)buf.data();
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, aa, &size) != NO_ERROR)
        return out;
    for (auto* a = aa; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            char ip[64];
            auto* sin = (sockaddr_in*)u->Address.lpSockaddr;
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof ip);
            out.push_back(ip);
        }
    }
    return out;
}

// ------------------------------------------------------------------ Art-Net
ArtNet::~ArtNet() {
    if (sock_ != ~0ull) closesocket((SOCKET)sock_);
}

bool ArtNet::send(const std::string& ip, int universe, const unsigned char* data, int count) {
    if (!netInit()) return false;
    if (sock_ == ~0ull) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return false;
        BOOL on = TRUE;
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof on);
        sock_ = s;
    }
    count = std::max(2, std::min(512, count + (count & 1)));  // length must be even, 2..512
    std::vector<unsigned char> p(18 + count, 0);
    memcpy(p.data(), "Art-Net\0", 8);
    p[8] = 0x00; p[9] = 0x50;    // OpDmx 0x5000, little-endian
    p[10] = 0; p[11] = 14;       // protocol version 14
    p[12] = ++seq_ ? seq_ : ++seq_;  // sequence (0 = disabled, so skip it)
    p[13] = 0;                   // physical
    p[14] = universe & 0xFF;     // SubUni
    p[15] = (universe >> 8) & 0x7F;  // Net
    p[16] = (count >> 8) & 0xFF; p[17] = count & 0xFF;
    memcpy(p.data() + 18, data, std::min(count, 512));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(6454);
    if (inet_pton(AF_INET, ip.c_str(), &a.sin_addr) != 1) return false;
    bool ok = sendto((SOCKET)sock_, (const char*)p.data(), (int)p.size(), 0, (sockaddr*)&a, sizeof a) > 0;
    if (ok) sent++;
    return ok;
}

// ------------------------------------------------------------------ UdpProbe
bool UdpProbe::open(int port) {
    close();
    if (!netInit()) return false;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) { closesocket(s); return false; }
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    sock_ = s;
    return true;
}
bool UdpProbe::receive(std::vector<unsigned char>& out) {
    if (sock_ == ~0ull) return false;
    unsigned char buf[2048];
    int n = recv((SOCKET)sock_, (char*)buf, sizeof buf, 0);
    if (n <= 0) return false;
    out.assign(buf, buf + n);
    return true;
}
void UdpProbe::close() {
    if (sock_ != ~0ull) closesocket((SOCKET)sock_);
    sock_ = ~0ull;
}

// ------------------------------------------------------------------ perf-test HTTP client
double httpRequestMs(int port, const std::string& path, const char* method) {
    if (!netInit()) return -1;
    double t0 = nowSec();
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof a) != 0) { closesocket(s); return -1; }
    std::string req = std::string(method) + " " + path + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(s, req.data(), (int)req.size(), 0);
    char buf[8192];
    bool got = false;
    while (true) {
        int n = recv(s, buf, sizeof buf, 0);
        if (n <= 0) break;
        got = true;
    }
    closesocket(s);
    return got ? (nowSec() - t0) * 1000.0 : -1;
}
