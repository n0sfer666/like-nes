#include "net_relay_server.hpp"

#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>

#include "platform_args.hpp"

namespace {

using Clock = std::chrono::steady_clock;
namespace udp = platform::net;
volatile std::sig_atomic_t stopping = 0;

void shutdown(int) { stopping = 1; }

void counters(const net::RelayStats& stats) {
    std::printf("relay packets %llu bytes %llu limited %llu rejected %llu forwarded %llu created %llu expired %llu\n",
        static_cast<unsigned long long>(stats.packets), static_cast<unsigned long long>(stats.bytes),
        static_cast<unsigned long long>(stats.limited), static_cast<unsigned long long>(stats.rejected),
        static_cast<unsigned long long>(stats.forwarded), static_cast<unsigned long long>(stats.created),
        static_cast<unsigned long long>(stats.expired));
    std::fflush(stdout);
}

bool drain(net::RelayServer& server, udp::Socket& socket, udp::Socket& other, uint64_t now) {
    if (!socket.valid()) return true;
    for (int i = 0; i < 64; ++i) {
        uint8_t bytes[udp::MAX_DATAGRAM];
        udp::Endpoint from;
        const int n = socket.recv_from(bytes, sizeof(bytes), &from);
        if (n <= 0) return n == 0;
        auto reply = server.receive(from, {bytes, static_cast<size_t>(n)}, now);
        if (!reply) continue;
        auto& send = reply->to.family == socket.local().family ? socket : other;
        send.send_to(reply->to, reply->packet.bytes, reply->packet.size);
    }
    return true;
}

bool port_arg(int argc, char** argv, uint16_t& port, bool& loopback) {
    if (argc == 1) return true;
    if (argc == 3 && std::strcmp(argv[2], "--loopback") == 0) loopback = true;
    else if (argc != 2) return false;
    const auto* end = argv[1] + std::strlen(argv[1]);
    const auto parsed = std::from_chars(argv[1], end, port);
    return parsed.ec == std::errc{} && parsed.ptr == end && port != 0;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    uint16_t port = 7777;
    bool loopback = false;
    if (!port_arg(argc, argv, port, loopback)) {
        std::fprintf(stderr, "usage: like_nes_relay [port [--loopback]]\n");
        return 2;
    }
    udp::Socket v4, v6;
    udp::Endpoint bind6;
    bind6.family = udp::AddressFamily::IPv6;
    bind6.port = port;
    if (loopback) bind6.address_v6.back() = 1;
    v4.open(loopback ? udp::ADDRESS_LOOPBACK : udp::ADDRESS_ANY, port);
    v6.open(bind6);
    if (!v4.valid() && !v6.valid()) {
        std::fprintf(stderr, "relay: cannot bind UDP port\n");
        return 1;
    }
    std::signal(SIGTERM, shutdown);
    std::signal(SIGINT, shutdown);
    std::printf("relay ready port %u families %u\n", static_cast<unsigned>(port),
        static_cast<unsigned>(v4.valid()) + static_cast<unsigned>(v6.valid()));
    std::fflush(stdout);
    net::RelayServer server;
    const auto start = Clock::now();
    uint64_t report_at = 5'000;
    uint64_t expire_at = 1'000;
    while (!stopping) {
        const uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start).count());
        if (!drain(server, v4, v6, now) || !drain(server, v6, v4, now)) {
            std::fprintf(stderr, "relay: socket receive failed\n");
            return 1;
        }
        if (now >= expire_at) {
            server.expire(now);
            expire_at = now + 1'000;
        }
        if (now >= report_at) {
            counters(server.stats());
            report_at = now + 5'000;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    counters(server.stats());
}
