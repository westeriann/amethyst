// Network control, polled from the main loop (no threads):
//  * OSC over UDP:  /amethyst/<action> [float|int]   e.g. /amethyst/next, /amethyst/crossfade 0.5
//  * HTTP remote:   a phone-friendly control page that drives the same actions.
#pragma once
#include <functional>
#include <string>
#include <vector>

class OscServer {
public:
    ~OscServer() { stop(); }
    bool start(int port, std::string& err);
    void stop();
    bool running() const { return sock_ != ~0ull; }
    int port() const { return port_; }
    // cb(address, hasValue, value)
    void poll(const std::function<void(const std::string&, bool, float)>& cb);
    std::string lastMessage;

private:
    unsigned long long sock_ = ~0ull;
    int port_ = 0;
};

struct HttpResponse {
    std::string contentType = "text/plain; charset=utf-8";
    std::string body;
    int status = 200;
};

class HttpServer {
public:
    ~HttpServer() { stop(); }
    bool start(int port, std::string& err);
    void stop();
    bool running() const { return sock_ != ~0ull; }
    int port() const { return port_; }
    // handler(method, path, query) -> response
    void poll(const std::function<HttpResponse(const std::string&, const std::string&, const std::string&)>& handler);
    static std::vector<std::string> localAddresses();

private:
    struct Client {
        unsigned long long sock;
        std::string buf;
        double started;
    };
    unsigned long long sock_ = ~0ull;
    int port_ = 0;
    std::vector<Client> clients_;
};

bool netInit();

// Art-Net (DMX over UDP, port 6454): drives lighting fixtures / DMX nodes.
class ArtNet {
public:
    ~ArtNet();
    // data: up to 512 channel values for one universe (Net/SubNet/Universe packed into 15 bits).
    bool send(const std::string& ip, int universe, const unsigned char* data, int count);
    int sent = 0;

private:
    unsigned long long sock_ = ~0ull;
    unsigned char seq_ = 0;
};

// Minimal UDP listener (used by the self-test to capture Art-Net packets on localhost).
class UdpProbe {
public:
    ~UdpProbe() { close(); }
    bool open(int port);
    bool receive(std::vector<unsigned char>& out);  // non-blocking
    void close();

private:
    unsigned long long sock_ = ~0ull;
};
