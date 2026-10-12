#pragma once
#include <memory>
#include <string>

// A small read-only web server on the local network for the players' phones.
// The app publishes the public state of the encounter as JSON; phones open the
// page (by scanning a QR code) and poll that state to show their character,
// the turn order and whose turn it is.
class PlayerServer {
public:
    PlayerServer();
    ~PlayerServer();

    // Starts listening on all interfaces, on the first free port from
    // `firstPort` (trying a few). Returns false if no port could be opened.
    bool start(int firstPort = 8080);
    void stop();
    bool running() const;
    int port() const;
    // http://<this computer's address on the local network>:<port>/
    std::string url() const;

    // Replaces the state the phones get (thread-safe; call it from the UI thread).
    void publish(const std::string& stateJson);
    // How many different phones asked for the state in the last few seconds.
    int viewers() const;

    struct Impl;  // implementation detail, defined in player_server.cpp

private:
    std::unique_ptr<Impl> impl;
};

// This computer's IPv4 address on the local network (e.g. "192.168.1.20"),
// or an empty string when there is no network.
std::string localNetworkAddress();
