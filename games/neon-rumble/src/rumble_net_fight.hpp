#pragma once
#include "rumble_net_input.hpp"
#include "session.hpp"

namespace rumble {

constexpr uint64_t NETWORK_SILENCE_MS = 500;
constexpr uint64_t NETWORK_RECONNECT_MS = 30'000;
constexpr uint64_t NETWORK_START_MS = 10'000;

enum class NetworkPhase { Starting, Playing, Paused, Solo, Desynced, Failed };

class NetworkFight {
public:
    using Send = void (*)(void* user, const platform::net::Endpoint& to, const net::Datagram& data);

    NetworkFight(Play& play, const NetworkSettings& settings, const net::MatchAgreement& match, uint64_t now);

    // Clock and networking remain live while advance() returns false.
    void receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now);
    void pump(uint64_t now, Send send, void* user);
    bool advance(PlayerCommand local, uint64_t now);
    const NetworkEffects* confirmed_effect();
    bool capture(uint32_t tick, NetworkSnapshot& out) const;

    NetworkPhase phase() const { return phase_; }
    uint32_t tick() const { return sim_.tick(); }
    uint32_t confirmed() const;
    uint32_t shared() const;
    uint32_t local_seat() const { return local_seat_; }
    uint32_t rollbacks() const { return rollback_.rollbacks(); }
    uint32_t resends() const { return link_.resent(); }
    uint32_t packets() const { return link_.sent(); }
    uint32_t reconnects() const { return reconnects_; }
    uint32_t rtt_ms() const { return rtt_ms_; }
    std::optional<uint32_t> mismatch() const { return mismatch_; }
    bool notifying(uint64_t now) const { return now < notice_until_; }
    uint64_t match_hash() const { return match_hash_; }
    uint32_t delay() const { return delay_; }
    const NetworkReplayFrame* replay_at(uint32_t tick) const;
    std::optional<uint64_t> hash_at(uint32_t tick) const { return sim_.hash_at(tick); }

private:
    struct Entry {
        uint32_t tick = UINT32_MAX;
        PlayerCommand command{};
    };
    struct Probe {
        uint32_t serial = 0;
        uint64_t at = 0;
    };

    using InputHistory = std::array<Entry, NETWORK_HISTORY>;
    void store(uint32_t tick, uint32_t seat, const PlayerCommand& command);
    void refresh_known();
    void expire(uint64_t now);
    void solo();
    bool active() const;
    void queue(uint64_t now);
    bool integrate(const NetworkInputPacket& p);
    bool proof(const NetworkInputPacket& p, uint64_t now);
    bool check_hashes(const NetworkInputPacket& p, uint64_t now);
    void desync(uint32_t tick, uint64_t now);
    void notify(uint64_t now, Send send, void* user);
    bool accept_notice(std::span<const uint8_t> bytes, uint64_t now);

    NetworkSimulation sim_;
    framework::rollback::Session<NetworkSimulation> rollback_;
    net::Link link_{80};
    net::SessionKey key_;
    std::array<InputHistory, br::SEATS> inputs_{};
    std::array<Probe, NETWORK_HISTORY> probes_{};
    NetworkPhase phase_ = NetworkPhase::Starting;
    uint32_t local_seat_ = 0;
    uint32_t remote_seat_ = 1;
    uint32_t delay_ = 0;
    uint32_t written_ = 0;
    uint32_t known_ = 0;
    uint32_t peer_known_ = 0;
    uint32_t published_ = 0;
    uint32_t probe_ = 0;
    uint32_t peer_probe_ = 0;
    uint32_t echo_ = 0;
    uint32_t rtt_ms_ = 0;
    uint32_t reconnects_ = 0;
    uint64_t entered_at_ = 0;
    uint64_t alive_at_ = 0;
    uint64_t next_send_ = 0;
    uint64_t match_hash_ = 0;
    uint64_t notice_until_ = 0;
    uint64_t next_notice_ = 0;
    std::optional<uint32_t> mismatch_;
};

} // namespace rumble
