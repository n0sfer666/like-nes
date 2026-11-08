#include "rumble_net_fight.hpp"

#include <algorithm>

namespace rumble {

NetworkFight::NetworkFight(Play& play, const NetworkSettings& settings,
                           const net::MatchAgreement& match, uint64_t now)
    : sim_(play, settings), key_(match.key), delay_(match.delay),
      entered_at_(now), alive_at_(now), next_send_(now), match_hash_(match.hash) {
    local_seat_ = key_.local_role == net::Role::Host ? 0u : 1u;
    remote_seat_ = 1 - local_seat_;
    if (delay_ < 1 || delay_ > 3 || play.scene.ticks != 0 ||
        encode_network_settings(settings) != match.data || net::match_hash(match.data, match.delay) != match.hash) {
        phase_ = NetworkPhase::Failed;
        return;
    }

    NetworkSnapshot initial;
    sim_.save(initial);
    rollback_.reset(br::SEATS, NETWORK_DEPTH, initial, NETWORK_HISTORY);
    PlayerCommand neutral{};
    neutral.input.present = true;
    for (uint32_t at = 0; at < delay_; ++at) {
        for (uint32_t p = 0; p < br::SEATS; ++p) store(at, p, neutral);
    }
    written_ = delay_;
    refresh_known();
}

void NetworkFight::store(uint32_t at, uint32_t seat, const PlayerCommand& command) {
    inputs_[seat][at % NETWORK_HISTORY] = {at, command};
    if (!rollback_.deliver(at, seat, command)) phase_ = NetworkPhase::Failed;
}

void NetworkFight::refresh_known() {
    while (known_ < written_ && inputs_[remote_seat_][known_ % NETWORK_HISTORY].tick == known_) ++known_;
}

uint32_t NetworkFight::shared() const { return std::min(known_, peer_known_); }

uint32_t NetworkFight::confirmed() const {
    return phase_ == NetworkPhase::Solo ? tick() : std::min(shared(), tick());
}

const NetworkReplayFrame* NetworkFight::replay_at(uint32_t at) const {
    return at <= confirmed() ? sim_.replay_at(at) : nullptr;
}

bool NetworkFight::active() const {
    return phase_ == NetworkPhase::Starting || phase_ == NetworkPhase::Playing || phase_ == NetworkPhase::Paused;
}

void NetworkFight::expire(uint64_t now) {
    if (phase_ == NetworkPhase::Starting && now - entered_at_ >= NETWORK_START_MS) phase_ = NetworkPhase::Failed;
    const uint64_t silence = std::clamp(uint64_t{rtt_ms_} * 3 + 200, NETWORK_SILENCE_MS, NETWORK_START_MS);
    if (phase_ == NetworkPhase::Playing && now - alive_at_ >= silence) {
        phase_ = NetworkPhase::Paused;
        entered_at_ = alive_at_ + silence;
    }
    if (phase_ == NetworkPhase::Paused && now - entered_at_ >= NETWORK_RECONNECT_MS) solo();
}

bool NetworkFight::capture(uint32_t at, NetworkSnapshot& out) const {
    if (at == tick()) {
        sim_.save(out);
        return true;
    }
    if (phase_ == NetworkPhase::Solo) return false;
    const auto* snapshot = rollback_.snapshot_before(at);
    if (!snapshot) return false;
    out = *snapshot;
    return true;
}

void NetworkFight::solo() {
    rollback_.settle(sim_);
    const uint32_t at = confirmed();
    if (at != tick()) {
        const auto* snapshot = rollback_.snapshot_before(at);
        if (!snapshot) {
            phase_ = NetworkPhase::Failed;
            return;
        }
        sim_.restore(*snapshot);
    }
    phase_ = NetworkPhase::Solo;
    key_ = {};
}

bool NetworkFight::advance(PlayerCommand local, uint64_t now) {
    expire(now);
    if (tick() - published_ > NETWORK_DEPTH) return false;
    local.input.present = true;
    if (phase_ == NetworkPhase::Solo) {
        std::array<PlayerCommand, br::SEATS> players{};
        players[local_seat_] = local;
        sim_.step(players.data());
        return true;
    }
    if (phase_ != NetworkPhase::Playing) return false;

    const uint32_t head = rollback_.tick();
    if (head >= UINT32_MAX - NETWORK_HISTORY) {
        phase_ = NetworkPhase::Failed;
        return false;
    }
    if (written_ == head + delay_) {
        store(written_, local_seat_, local);
        ++written_;
        refresh_known();
    }
    if (phase_ == NetworkPhase::Failed || head - std::min(head, shared()) >= NETWORK_DEPTH) return false;
    rollback_.advance(sim_);
    return true;
}

const NetworkEffects* NetworkFight::confirmed_effect() {
    if (published_ >= confirmed()) return nullptr;
    const auto& effect = sim_.effects(published_ + 1);
    if (effect.tick != published_ + 1) return nullptr;
    ++published_;
    return &effect;
}

} // namespace rumble
