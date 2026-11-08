#pragma once
#include "net_session_wire.hpp"

namespace net {

using RoomCode = uint64_t;
constexpr RoomCode MAX_ROOM_CODE = (RoomCode{1} << 40) - 1;
constexpr size_t RELAY_CONTROL_BYTES = 4 + 4 + 4 * Token{}.size() + 8 + 1 + 2 + 4 + 16;
constexpr size_t RELAY_ROUTE_BYTES = 4 + 4 + 2 * Token{}.size() + 2;
constexpr size_t RELAY_PAYLOAD = platform::net::MAX_DATAGRAM - RELAY_ROUTE_BYTES;

enum class RelayKind : uint8_t { Intent = 1, Challenge, Proof, Grant, Query, State, Refuse, Route, Forward };
enum class RelayAction : uint8_t { None, Create, Join, Rebind };
enum class RelayRefusal : uint8_t { None, Missing, Full, Busy, Random };

struct RelayControl {
    RelayKind kind = RelayKind::Intent;
    RelayAction action = RelayAction::None;
    Role role = Role::Host;
    RelayRefusal refusal = RelayRefusal::None;
    Token request{};
    Token challenge{};
    Token room{};
    Token key{};
    RoomCode code = 0;
    platform::net::Endpoint peer;
    bool operator==(const RelayControl&) const = default;
};

struct RelayRoute {
    RelayKind kind = RelayKind::Route;
    Role role = Role::Host;
    Token room{};
    Token key{};
    std::span<const uint8_t> payload;
};

bool encode_relay_control(const RelayControl& frame, Datagram& out);
bool decode_relay_control(std::span<const uint8_t> bytes, RelayControl& out);
bool encode_relay_route(const RelayRoute& frame, Datagram& out);
bool decode_relay_route(std::span<const uint8_t> bytes, RelayRoute& out);

} // namespace net
