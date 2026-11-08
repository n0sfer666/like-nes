#pragma once
#include "net_link.hpp"

#include <array>
#include <span>

namespace net {

using Token = std::array<uint8_t, 16>;
using MatchData = std::array<uint8_t, 32>;
bool random_token(Token& token);

constexpr uint16_t MATCH_VERSION = 1;
enum class Role : uint8_t { Host = 1, Guest = 2 };
enum class Refusal : uint8_t {
    None, Version, Data, Level, Difficulty, FriendlyFire, Settings, Timeout, Random
};
enum class LobbyKind : uint8_t { Hello = 1, Challenge, Echo, Offer, Ready, Start, Started, Reject };

constexpr uint8_t RTT_ROUNDS = 3;
constexpr size_t LOBBY_BYTES = 8 + 5 * Token{}.size() + MatchData{}.size() + 8 + 3;

// Fields unused by a message are zero. All lobby replies fit in one request.
struct LobbyFrame {
    uint16_t version = MATCH_VERSION;
    LobbyKind kind = LobbyKind::Hello;
    uint8_t round = 0;
    Token attempt{};
    Token proof{};
    Token session{};
    Token host_key{};
    Token guest_key{};
    MatchData data{};
    uint64_t match_hash = 0;
    uint8_t probe = 0;
    uint8_t delay = 0;
    Refusal refusal = Refusal::None;
    bool operator==(const LobbyFrame&) const = default;
};

bool encode_lobby(const LobbyFrame& frame, Datagram& out);
bool decode_lobby(std::span<const uint8_t> bytes, LobbyFrame& out);
uint64_t match_hash(const MatchData& data, uint8_t delay);

struct SessionKey {
    platform::net::Endpoint peer;
    Token session{};
    Token host_key{};
    Token guest_key{};
    Role local_role = Role::Host;
};

constexpr size_t SESSION_BYTES = 9 + 2 * Token{}.size();
constexpr size_t SESSION_PAYLOAD = MAX_PAYLOAD - SESSION_BYTES;
bool wrap_session(const SessionKey& key, std::span<const uint8_t> link, Datagram& out);
std::span<const uint8_t> unwrap_session(const SessionKey& key, const platform::net::Endpoint& from,
                                        std::span<const uint8_t> bytes);

} // namespace net
