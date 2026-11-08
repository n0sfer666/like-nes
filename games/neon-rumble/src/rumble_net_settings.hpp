#pragma once
#include "net_lobby.hpp"
#include "net_wire.hpp"

namespace rumble {

enum class Difficulty : uint8_t { Easy = 1, Normal, Hard };
constexpr uint32_t NETWORK_LEVEL1 = 1;

struct NetworkSettings {
    uint64_t library_hash = 0;
    uint64_t game_hash = 0;
    uint64_t seed = 0;
    uint32_t level = NETWORK_LEVEL1;
    Difficulty difficulty = Difficulty::Normal;
    bool friendly_fire = false;
    bool operator==(const NetworkSettings&) const = default;
};

inline net::MatchData encode_network_settings(const NetworkSettings& s) {
    net::MatchData data{};
    net::Writer w(data.data(), data.size());
    w.u64(s.library_hash);
    w.u64(s.game_hash);
    w.u64(s.seed);
    w.u32(s.level);
    w.u8(static_cast<uint8_t>(s.difficulty));
    w.u8(s.friendly_fire ? 1 : 0);
    w.u16(0);
    return data;
}

inline net::Refusal decode_network_settings(const net::MatchData& data, NetworkSettings& out) {
    net::Reader r(data.data(), data.size());
    NetworkSettings s;
    uint8_t difficulty = 0, friendly = 0;
    uint16_t reserved = 0;
    r.u64(&s.library_hash);
    r.u64(&s.game_hash);
    r.u64(&s.seed);
    r.u32(&s.level);
    r.u8(&difficulty);
    r.u8(&friendly);
    r.u16(&reserved);
    if (!r.ok() || reserved != 0) return net::Refusal::Settings;
    if (s.level != NETWORK_LEVEL1) return net::Refusal::Level;
    if (difficulty < static_cast<uint8_t>(Difficulty::Easy) ||
        difficulty > static_cast<uint8_t>(Difficulty::Hard)) return net::Refusal::Difficulty;
    if (friendly > 1) return net::Refusal::FriendlyFire;
    s.difficulty = static_cast<Difficulty>(difficulty);
    s.friendly_fire = friendly != 0;
    out = s;
    return net::Refusal::None;
}

inline net::Refusal compatible_network_settings(const net::MatchData& local, const net::MatchData& peer) {
    NetworkSettings theirs;
    const auto reason = decode_network_settings(peer, theirs);
    if (reason != net::Refusal::None) return reason;
    NetworkSettings ours;
    const auto local_reason = decode_network_settings(local, ours);
    if (local_reason != net::Refusal::None) return local_reason;
    if (ours.library_hash != theirs.library_hash || ours.game_hash != theirs.game_hash) {
        return net::Refusal::Data;
    }
    return net::Refusal::None;
}

inline net::LobbyConfig network_lobby_config(const NetworkSettings& settings) {
    return {encode_network_settings(settings), compatible_network_settings};
}

} // namespace rumble
