#pragma once
#include "net_relay_wire.hpp"

#include <optional>

namespace net {

constexpr size_t RELAY_ROOMS = 64;
constexpr size_t RELAY_CHALLENGES = 128;
constexpr size_t RELAY_RATE_SOURCES = 256;
constexpr uint64_t RELAY_ROOM_MS = 60'000;
constexpr uint64_t RELAY_CHALLENGE_MS = 5'000;
constexpr uint32_t RELAY_PACKETS_PER_IP = 256;
constexpr uint32_t RELAY_BYTES_PER_IP = 128 * 1024;

struct RelayStats {
    uint64_t packets = 0;
    uint64_t bytes = 0;
    uint64_t limited = 0;
    uint64_t rejected = 0;
    uint64_t forwarded = 0;
    uint64_t created = 0;
    uint64_t expired = 0;
};

struct RelaySend {
    platform::net::Endpoint to;
    Datagram packet;
};

class RelayServer {
public:
    using Random = bool (*)(Token&);
    explicit RelayServer(Random random = random_token) : random_(random) {}

    std::optional<RelaySend> receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now);
    void expire(uint64_t now);
    const RelayStats& stats() const { return stats_; }

private:
    using Endpoint = platform::net::Endpoint;

    struct Member {
        Endpoint endpoint;
        Token key{};
        Token request{};
        uint64_t binding = 0;
    };
    struct Room {
        Token id{};
        RoomCode code = 0;
        std::array<Member, 2> members{};
        uint64_t touched = 0;
    };
    struct Candidate {
        Endpoint endpoint;
        RelayControl intent;
        Token challenge{};
        uint64_t binding = 0;
        uint64_t deadline = 0;
        std::optional<RelayControl> response;
    };
    struct Budget {
        uint64_t window = 0;
        uint32_t packets = 0;
        uint32_t bytes = 0;
        bool admit(uint64_t now, uint32_t size, uint32_t max_packets, uint32_t max_bytes);
    };
    struct Source {
        Endpoint ip;
        uint64_t expires = 0;
        Budget budget;
    };

    bool admit(const Endpoint& from, uint32_t size, uint64_t now);
    bool random(Token& out) const { return random_ && random_(out) && out != Token{}; }
    Room* find(const Token& id);
    Room* find(RoomCode code);
    RelayControl action(const Candidate& request, uint64_t now);
    RelayRefusal create(const Candidate& request, Room*& room, uint64_t now);
    RelayRefusal join(const Candidate& request, Room*& room);
    RelayRefusal rebind(const Candidate& request, Room*& room);
    std::optional<RelaySend> intent(const Endpoint& from, const RelayControl& f, uint64_t now);
    std::optional<RelaySend> proof(const Endpoint& from, const RelayControl& f, uint64_t now);
    std::optional<RelaySend> query(const Endpoint& from, const RelayControl& f, uint64_t now);
    std::optional<RelaySend> route(const Endpoint& from, const RelayRoute& f, uint64_t now);
    static std::optional<RelaySend> send(const Endpoint& to, const RelayControl& frame);

    Random random_;
    RelayStats stats_;
    Budget global_;
    std::array<Source, RELAY_RATE_SOURCES> sources_{};
    std::array<Candidate, RELAY_CHALLENGES> candidates_{};
    std::array<Room, RELAY_ROOMS> rooms_{};
};

} // namespace net
