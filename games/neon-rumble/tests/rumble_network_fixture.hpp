#pragma once
#include "rumble_net_fight.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace rumble::test {

namespace udp = platform::net;
constexpr uint32_t TARGET = 850;
constexpr uint32_t CUT = 6'500;
constexpr uint8_t DELAY = 2;
using InputTape = std::array<std::array<PlayerCommand, 2>, TARGET>;
constexpr std::array<udp::Endpoint, 2> ENDPOINTS{{
    {udp::ADDRESS_LOOPBACK, 4990}, {udp::ADDRESS_LOOPBACK, 4991}
}};

inline void require(bool ok, const char* reason) {
    if (ok) return;
    std::fprintf(stderr, "rumble-network-sim: FAIL %s\n", reason);
    std::exit(1);
}

inline std::unique_ptr<Play> level(const asset::BundleView& game) {
    std::string error;
    auto play = enter_level(game, "level1", true, error);
    require(play != nullptr, error.c_str());
    return play;
}

inline PlayerCommand input(uint32_t tick, uint32_t seat) {
    PlayerCommand c;
    c.input.present = true;
    c.input.move.move_x = fix32::from_int(tick < 520 ? 1 : -1);
    c.input.move.move_z = fix32::from_int((tick + seat * 17) % 48 < 24 ? 1 : -1);
    if ((tick + seat * 11) % 30 == 5) c.attack = Attack::Kick;
    return c;
}

inline uint64_t peer_clock(uint32_t t, uint32_t peer) {
    return peer == 0 ? 80'000 + uint64_t{t} : 1150 + uint64_t{t} + t / 1000;
}

inline net::MatchAgreement match(NetworkSettings settings, uint32_t peer) {
    net::MatchAgreement m;
    m.data = encode_network_settings(settings);
    m.delay = DELAY;
    m.hash = net::match_hash(m.data, m.delay);
    m.key.peer = ENDPOINTS[1 - peer];
    m.key.local_role = peer == 0 ? net::Role::Host : net::Role::Guest;
    m.key.session[0] = 1;
    m.key.host_key[0] = 2;
    m.key.guest_key[0] = 3;
    return m;
}

struct Peer {
    std::unique_ptr<Play> play;
    NetworkFight fight;
    uint32_t emitted = 0;
    uint32_t hits = 0;
    uint32_t spawns = 0;
    bool paused = false;
    uint32_t solo_tick = 0;
    uint32_t pause_at = 0;
    uint32_t solo_at = 0;

    Peer(const asset::BundleView& game, NetworkSettings settings, uint32_t seat)
        : play(level(game)), fight(*play, settings, match(settings, seat), peer_clock(0, seat)) {}
};

enum class Path { Clean, Mixed, HighLag, Recover, RecoverOneWay, Solo, SoloOneWay };

struct Pending {
    uint32_t target;
    uint32_t due;
    net::Datagram packet;
};

struct Pair {
    Peer a, b;
    std::array<Peer*, 2> peers{&a, &b};
    const InputTape& tape;
    Path path;
    uint32_t now = 0;
    uint32_t sent = 0;
    uint32_t lost = 0;
    uint32_t duplicated = 0;
    std::vector<Pending> pending;
    std::array<std::optional<Pending>, 2> stale;
    bool delayed_echo = false;

    Pair(const asset::BundleView& game, NetworkSettings terms, const InputTape& commands, Path route)
        : a(game, terms, 0), b(game, terms, 1), tape(commands), path(route) {}

    void send(uint32_t from, const net::Datagram& packet) {
        ++sent;
        if (now >= CUT && path >= Path::Recover) {
            const bool one_way = path == Path::RecoverOneWay || path == Path::SoloOneWay;
            const bool permanent = path == Path::Solo || path == Path::SoloOneWay;
            if ((!one_way || from == 0) && (permanent || now < CUT + 1100)) {
                const auto inner = net::unwrap_session(match({}, 1 - from).key, ENDPOINTS[from],
                    {packet.bytes, packet.size});
                NetworkInputPacket decoded;
                if (!stale[1 - from] && inner.size() > net::HEADER_BYTES &&
                    decode_network_input(inner.subspan(net::HEADER_BYTES), decoded) && decoded.echo) {
                    stale[1 - from] = Pending{1 - from, now, packet};
                }
                ++lost;
                return;
            }
        }
        const bool noise = path == Path::Mixed;
        if (noise && sent % 11 == 0) {
            ++lost;
            return;
        }
        const uint32_t lag = noise ? (sent % 3 == 0 ? 53u : sent % 3 == 1 ? 2u : 22u) :
            path == Path::HighLag ? 260u : 2u;
        pending.push_back({1 - from, now + lag, packet});
        if (noise && sent % 13 == 0) {
            pending.push_back({1 - from, now + lag + 5, packet});
            ++duplicated;
        }
    }

    void inject(const Pending& p) {
        peers[p.target]->fight.receive(ENDPOINTS[1 - p.target],
            {p.packet.bytes, p.packet.size}, peer_clock(now, p.target));
    }

    void step() {
        if (path == Path::SoloOneWay && !delayed_echo && b.paused && stale[1]) {
            inject(*stale[1]);
            require(b.fight.phase() == NetworkPhase::Paused, "old echo cannot revive a broken path");
            delayed_echo = true;
        }
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due > now) { ++it; continue; }
            inject(*it);
            it = pending.erase(it);
        }
        for (uint32_t seat = 0; seat < peers.size(); ++seat) {
            Peer& p = *peers[seat];
            const auto clock = peer_clock(now, seat);
            struct Tx { Pair& pair; uint32_t seat; } tx{*this, seat};
            p.fight.pump(clock, [](void* user, const udp::Endpoint& to, const net::Datagram& packet) {
                auto& out = *static_cast<Tx*>(user);
                require(to == ENDPOINTS[1 - out.seat], "peer endpoint");
                out.pair.send(out.seat, packet);
            }, &tx);
            if (now % 16 == 0 && p.fight.tick() < TARGET) p.fight.advance(tape[p.fight.tick()][seat], clock);
            const auto phase = p.fight.phase();
            require(phase != NetworkPhase::Failed && phase != NetworkPhase::Desynced, "fight remains coherent");
            if (phase == NetworkPhase::Paused && !p.paused) {
                p.paused = true;
                p.pause_at = now;
            }
            if (phase == NetworkPhase::Solo && p.solo_at == 0) {
                p.solo_at = now;
                p.solo_tick = p.fight.tick();
            }
            if (phase != NetworkPhase::Solo) {
                require(p.fight.tick() <= p.fight.shared() + NETWORK_DEPTH, "prediction lead <= 8");
            }
            while (const auto* effect = p.fight.confirmed_effect()) {
                require(effect->tick == ++p.emitted, "confirmed effects are emitted exactly once");
                for (uint32_t i = 0; i < effect->hits.count; ++i) p.hits += effect->hits.at[i].kept ? 1 : 0;
                p.spawns += effect->waves.count;
            }
        }
    }

    void run() {
        const uint32_t timeout = path == Path::HighLag ? 150'000 : 60'000;
        for (; now < timeout; ++now) {
            step();
            if (a.emitted == TARGET && b.emitted == TARGET) return;
        }
        std::fprintf(stderr, "route %d: ticks %u/%u, confirmed %u/%u\n", static_cast<int>(path),
                     a.fight.tick(), b.fight.tick(), a.fight.confirmed(), b.fight.confirmed());
        require(false, "match reached target on independent clocks");
    }
};

} // namespace rumble::test
