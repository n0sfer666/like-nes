#pragma once
#include "net_relay_server.hpp"

#include <cstdio>
#include <cstdlib>

namespace relay_test {
using namespace net;
using platform::net::Endpoint;

inline void require(bool ok, const char* message) {
    if (ok) return;
    std::fprintf(stderr, "relay: FAIL %s\n", message);
    std::exit(1);
}

inline bool serial(Token& token) {
    static uint32_t next = 0;
    ++next;
    token = {};
    for (size_t i = 0; i < 4; ++i) token[i] = static_cast<uint8_t>(next >> (i * 8));
    return true;
}

inline RelayControl start(RelayAction action, Role role) {
    RelayControl request;
    request.action = action;
    request.role = role;
    serial(request.request);
    return request;
}

inline std::optional<RelaySend> receive(RelayServer& server, Endpoint from, const Datagram& request, uint64_t now) {
    auto reply = server.receive(from, {request.bytes, request.size}, now);
    require(!reply || reply->packet.size <= request.size, "single reply does not amplify");
    return reply;
}

inline std::optional<RelayControl> call(RelayServer& server, Endpoint from, const RelayControl& f, uint64_t now) {
    Datagram request;
    require(encode_relay_control(f, request), "request encodes");
    auto reply = receive(server, from, request, now);
    if (!reply) return {};
    require(reply->to == from, "reply goes to request source");
    RelayControl control;
    require(decode_relay_control({reply->packet.bytes, reply->packet.size}, control), "reply decodes");
    return control;
}

inline RelayControl proof(RelayServer& server, Endpoint from, RelayControl request, uint64_t now) {
    auto challenge = call(server, from, request, now);
    require(challenge && challenge->kind == RelayKind::Challenge &&
            challenge->code == 0 && challenge->room == Token{} && challenge->key == Token{},
            "challenge contains only proof material");
    request.kind = RelayKind::Proof;
    request.challenge = challenge->challenge;
    return request;
}

inline RelayControl grant(RelayServer& server, Endpoint from, const RelayControl& request, uint64_t now) {
    auto reply = call(server, from, request, now);
    require(reply && reply->kind == RelayKind::Grant, "verified request grants room");
    return *reply;
}

} // namespace relay_test
