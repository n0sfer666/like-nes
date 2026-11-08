#pragma once
#include "net_session_wire.hpp"

#include <optional>

namespace net {

constexpr uint64_t LOBBY_RETRY_MS = 150;
constexpr uint64_t LOBBY_TIMEOUT_MS = 10'000;
constexpr uint64_t LOBBY_READY_TIMEOUT_MS = 60'000;
constexpr size_t LOBBY_CANDIDATES = 8;

struct LobbyConfig {
    MatchData data{};
    // Compatibility callbacks return None or a reason in Data..Settings.
    Refusal (*check)(const MatchData& local, const MatchData& remote) = nullptr;
};

struct LobbySend {
    platform::net::Endpoint to;
    Datagram datagram;
};

struct MatchAgreement {
    SessionKey key;
    MatchData data{};
    uint64_t hash = 0;
    uint8_t delay = 0;
};

enum class LobbyPhase { Waiting, Probing, Offered, Starting, Started, Rejected };

// The owner supplies monotonic milliseconds, calls poll on network time even
// when simulation is paused, and keeps servicing the final Started reply.
class Lobby {
public:
    using Random = bool (*)(Token&);

    explicit Lobby(LobbyConfig config, Random random = random_token)
        : config_(config), random_(random) {}

    void listen();
    bool join(const platform::net::Endpoint& host, uint64_t now);
    void ready() { local_ready_ = true; }
    void receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now);
    std::optional<LobbySend> poll(uint64_t now);

    LobbyPhase phase() const { return phase_; }
    Refusal refusal() const { return refusal_; }
    const std::optional<MatchAgreement>& agreement() const { return agreement_; }

private:
    struct Candidate {
        static constexpr size_t MAX_PROBES = LOBBY_TIMEOUT_MS / LOBBY_RETRY_MS + 1;
        static_assert(MAX_PROBES <= UINT8_MAX);
        platform::net::Endpoint peer;
        LobbyFrame frame;
        MatchData data{};
        std::array<uint64_t, RTT_ROUNDS> rtt{};
        std::array<uint64_t, MAX_PROBES> sent_at{};
        uint64_t deadline = 0;
        uint64_t next = 0;
        bool due = false;
    };

    void expire(uint64_t now);
    void reject(Refusal reason);
    Refusal check_settings(const MatchData& remote) const;
    bool new_token(Token& out) { return random_ && random_(out) && out != Token{}; }
    void receive_host(const platform::net::Endpoint& from, const LobbyFrame& f, uint64_t now);
    void receive_guest(const LobbyFrame& f, uint64_t now);
    void hello(const platform::net::Endpoint& from, const LobbyFrame& f, uint64_t now);
    void echo(const platform::net::Endpoint& from, const LobbyFrame& f, uint64_t now);
    void offer(Candidate& candidate, uint64_t now);
    LobbyFrame control(LobbyKind kind, Role sender) const;
    std::optional<LobbySend> send(const platform::net::Endpoint& to, const LobbyFrame& f);
    std::optional<LobbySend> poll_host(uint64_t now);
    std::optional<LobbySend> poll_candidate(uint64_t now);
    std::optional<LobbySend> poll_guest(uint64_t now);

    LobbyConfig config_;
    Random random_;
    Role role_ = Role::Host;
    LobbyPhase phase_ = LobbyPhase::Waiting;
    Refusal refusal_ = Refusal::None;
    bool local_ready_ = false;
    bool remote_ready_ = false;
    bool reply_due_ = false;
    platform::net::Endpoint peer_{};
    LobbyFrame pending_{};
    LobbyFrame proposal_{};
    std::optional<MatchAgreement> agreement_;
    uint64_t next_ = 0;
    uint64_t deadline_ = 0;
    uint64_t admit_after_ = 0;
    size_t cursor_ = 0;
    std::array<Candidate, LOBBY_CANDIDATES> candidates_{};
};

} // namespace net
