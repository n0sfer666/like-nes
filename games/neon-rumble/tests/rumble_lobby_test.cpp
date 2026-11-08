#include "rumble_net_settings.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <tuple>

namespace {
using namespace rumble;
using namespace net;
namespace udp = platform::net;
using Clock = std::chrono::steady_clock;

void require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "rumble-lobby: FAIL %s\n", what);
    std::exit(1);
}

const NetworkSettings HOST_SETTINGS{0x12345678u, 0xfedcba98u, 0x1020304050607080ull,
                                    NETWORK_LEVEL1, Difficulty::Hard, true};

void settings_codec() {
    const auto data = encode_network_settings(HOST_SETTINGS);
    NetworkSettings out;
    require(decode_network_settings(data, out) == Refusal::None && out == HOST_SETTINGS,
            "settings round-trip");
    const auto hash = match_hash(data, 2);
    for (size_t offset : {size_t{0}, size_t{8}, size_t{16}, size_t{24}, size_t{28}, size_t{29}}) {
        auto changed = data;
        changed[offset] ^= 1;
        require(match_hash(changed, 2) != hash, "every field changes the session hash");
    }
    for (auto [offset, value, reason] : {
            std::tuple{size_t{0}, uint8_t{2}, Refusal::Data},
            std::tuple{size_t{8}, uint8_t{2}, Refusal::Data},
            std::tuple{size_t{24}, uint8_t{2}, Refusal::Level},
            std::tuple{size_t{28}, uint8_t{0}, Refusal::Difficulty},
            std::tuple{size_t{29}, uint8_t{2}, Refusal::FriendlyFire},
            std::tuple{size_t{30}, uint8_t{1}, Refusal::Settings}}) {
        auto invalid = data;
        invalid[offset] = value;
        require(compatible_network_settings(data, invalid) == reason, "specific incompatibility");
    }
}

struct Peer {
    udp::Socket socket;
    Lobby lobby;
    Clock::time_point start = Clock::now();

    explicit Peer(LobbyConfig config) : lobby(config) {}
    uint64_t now() const {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start).count());
    }

    void pump() {
        uint8_t bytes[udp::MAX_DATAGRAM];
        udp::Endpoint from;
        for (int i = 0; i < 32; ++i) {
            int n = socket.recv_from(bytes, sizeof(bytes), &from);
            require(n >= 0, "UDP receive");
            if (n == 0) break;
            lobby.receive(from, {bytes, static_cast<size_t>(n)}, now());
        }
        if (auto send = lobby.poll(now())) {
            require(socket.send_to(send->to, send->datagram.bytes, send->datagram.size), "UDP send");
        }
    }
};

void loopback(const char* address, bool compatible) {
    udp::Endpoint bind;
    require(udp::parse_endpoint(address, &bind), "parse local bind");
    bind.port = 0;
    auto guest_settings = HOST_SETTINGS;
    guest_settings.seed = 0;
    guest_settings.difficulty = Difficulty::Easy;
    guest_settings.friendly_fire = false;
    if (!compatible) ++guest_settings.game_hash;

    Peer host(network_lobby_config(HOST_SETTINGS));
    Peer guest(network_lobby_config(guest_settings));
    require(host.socket.open(bind) && guest.socket.open(bind), "two real UDP sockets");
    require(guest.lobby.join(host.socket.local(), guest.now()), "guest joins");
    guest.lobby.ready();
    bool offer_seen = false;
    while (host.now() < 4'000) {
        host.pump();
        guest.pump();
        if (guest.lobby.phase() == LobbyPhase::Offered && !offer_seen) {
            require(host.lobby.phase() == LobbyPhase::Offered, "guest alone cannot start the match");
            offer_seen = true;
            host.lobby.ready();
        }
        if (host.lobby.phase() == LobbyPhase::Started || guest.lobby.phase() == LobbyPhase::Rejected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!compatible) {
        require(guest.lobby.refusal() == Refusal::Data && !guest.lobby.agreement() &&
                !host.lobby.agreement(), "incompatible build refused over UDP");
        return;
    }
    const auto& a = host.lobby.agreement();
    const auto& b = guest.lobby.agreement();
    require(host.lobby.phase() == LobbyPhase::Started && guest.lobby.phase() == LobbyPhase::Started &&
            a && b && a->hash == b->hash && a->delay == b->delay && a->delay >= 1 && a->delay <= 3,
            "both real peers start the same match");
    NetworkSettings agreed;
    require(decode_network_settings(b->data, agreed) == Refusal::None && agreed == HOST_SETTINGS,
            "guest confirmed exact host rules");
}

} // namespace

int main() {
    settings_codec();
    loopback("127.0.0.1:1", true);
    loopback("[::1]:1", true);
    loopback("127.0.0.1:1", false);
    std::puts("rumble-lobby: PASS");
}
