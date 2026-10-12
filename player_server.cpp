#include "player_server.h"

#include <chrono>
#include <map>
#include <mutex>
#include <thread>

#include "httplib.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

// The page the phones load (HTML, CSS and JavaScript in one), as PLAYER_PAGE_PARTS.
#include "player_page.inc"

std::string pageHtml() {
    std::string page;
    for (const char* part : PLAYER_PAGE_PARTS) page += part;
    return page;
}

bool isPrivateIPv4(const std::string& ip) {
    return ip.rfind("192.168.", 0) == 0 || ip.rfind("10.", 0) == 0 ||
           (ip.rfind("172.", 0) == 0 && [&] {
               const int second = std::atoi(ip.c_str() + 4);
               return second >= 16 && second <= 31;
           }());
}

}  // namespace

std::string localNetworkAddress() {
    // Ask the system which local address it would use to reach the outside
    // world. A UDP "connect" sends nothing; it only picks the route.
    std::string found;
#ifdef _WIN32
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != INVALID_SOCKET) {
#else
    int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
#endif
        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
        if (::connect(s, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
            sockaddr_in local{};
            socklen_t len = sizeof(local);
            if (::getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
                char buf[INET_ADDRSTRLEN] = {0};
                if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) found = buf;
            }
        }
#ifdef _WIN32
        ::closesocket(s);
#else
        ::close(s);
#endif
    }
    if (!found.empty() && found != "0.0.0.0" && found.rfind("127.", 0) != 0) return found;

#ifndef _WIN32
    // No route out (e.g. a Wi-Fi with no internet): pick a private address from
    // the network interfaces instead.
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (ifaddrs* it = list; it; it = it->ifa_next) {
            if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
            char buf[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(it->ifa_addr)->sin_addr, buf, sizeof(buf));
            if (isPrivateIPv4(buf)) {
                found = buf;
                break;
            }
        }
        freeifaddrs(list);
    }
#endif
    return isPrivateIPv4(found) ? found : std::string();
}

struct PlayerServer::Impl {
    httplib::Server server;
    std::thread thread;
    int port = 0;
    bool running = false;

    mutable std::mutex mutex;
    std::string state = "{}";
    std::map<std::string, std::chrono::steady_clock::time_point> lastSeen;  // by phone address
};

PlayerServer::PlayerServer() : impl(std::make_unique<Impl>()) {}

PlayerServer::~PlayerServer() { stop(); }

bool PlayerServer::start(int firstPort) {
    if (impl->running) return true;
    Impl* d = impl.get();
    const std::string page = pageHtml();

    d->server.Get("/", [page](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-store");
        res.set_content(page, "text/html; charset=utf-8");
    });
    d->server.Get("/state", [d](const httplib::Request& req, httplib::Response& res) {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(d->mutex);
            body = d->state;
            d->lastSeen[req.remote_addr] = std::chrono::steady_clock::now();
        }
        res.set_header("Cache-Control", "no-store");
        res.set_content(body, "application/json; charset=utf-8");
    });
    d->server.Get("/favicon.ico", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });
    d->server.set_read_timeout(5, 0);
    d->server.set_write_timeout(5, 0);

    for (int port = firstPort; port < firstPort + 20; port++) {
        if (d->server.bind_to_port("0.0.0.0", port)) {
            d->port = port;
            d->running = true;
            d->thread = std::thread([d] { d->server.listen_after_bind(); });
            return true;
        }
    }
    return false;
}

void PlayerServer::stop() {
    if (!impl->running) return;
    impl->server.stop();
    if (impl->thread.joinable()) impl->thread.join();
    impl->running = false;
    // A stopped httplib::Server can't listen again; start over with a fresh one.
    impl = std::make_unique<Impl>();
}

bool PlayerServer::running() const { return impl->running; }

int PlayerServer::port() const { return impl->port; }

std::string PlayerServer::url() const {
    std::string host = localNetworkAddress();
    if (host.empty()) host = "localhost";
    return "http://" + host + ":" + std::to_string(impl->port) + "/";
}

void PlayerServer::publish(const std::string& stateJson) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state = stateJson;
}

int PlayerServer::viewers() const {
    std::lock_guard<std::mutex> lock(impl->mutex);
    const auto now = std::chrono::steady_clock::now();
    int n = 0;
    for (const auto& seen : impl->lastSeen) {
        if (now - seen.second < std::chrono::seconds(5)) n++;
    }
    return n;
}
