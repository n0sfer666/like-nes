#include "rumble_net_mode.hpp"

#include <cstdio>

#include "rumble_feedback.hpp"

namespace rumble {
namespace {

namespace udp = platform::net;

void send_fight(void* user, const udp::Endpoint& to, const net::Datagram& packet) {
    static_cast<NetworkChannel*>(user)->send(to, packet, true);
}

std::string room_name(uint64_t seed) {
    char text[48];
    std::snprintf(text, sizeof(text), "Neon Rumble %08llx", static_cast<unsigned long long>(seed & 0xffffffffu));
    return text;
}

} // namespace

NetworkMode::NetworkMode(Flow& flow, uint64_t library_hash, uint64_t game_hash, bool russian)
    : flow_(flow), russian_(russian) {
    settings_.library_hash = library_hash;
    settings_.game_hash = game_hash;
    discovery_.open({});
}

uint64_t NetworkMode::now() const {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count());
}

void NetworkMode::reset(std::string status) {
    fight_.reset();
    lobby_.reset();
    channel_.close();
    discovery_.close();
    discovery_.open({});
    if (flow_.play()) flow_.leave();
    opened_ = ready_ = entered_ = solo_ = false;
    edit_ = nullptr;
    status_ = std::move(status);
    dump_ = {};
}

void NetworkMode::start(std::span<const udp::Endpoint> endpoints, bool host, bool relay, net::RoomCode code) {
    status_.clear();
    if (endpoints.empty() || !channel_.open(endpoints, host, relay, code, false, now())) {
        reset("Could not open the address");
        return;
    }
    discovery_.close();
    if (host) {
        net::Token random{};
        if (!net::random_token(random)) {
            reset(lobby_error(net::Refusal::Random));
            return;
        }
        net::Reader r(random.data(), random.size());
        r.u64(&settings_.seed);
        if (!relay) discovery_.open({room_name(settings_.seed), endpoints[0].port});
    }
    host_ = host;
    relay_mode_ = relay;
    lobby_.emplace(network_lobby_config(settings_));
}

void NetworkMode::open_match(uint64_t clock) {
    if (!lobby_ || lobby_->phase() != net::LobbyPhase::Started || fight_) return;
    if (auto* play = flow_.play()) {
        const auto& agreed = *lobby_->agreement();
        NetworkSettings terms;
        if (decode_network_settings(agreed.data, terms) != net::Refusal::None) {
            reset("Incompatible match");
            return;
        }
        fight_ = std::make_unique<NetworkFight>(*play, terms, agreed, clock);
        next_tick_ = Clock::now();
        channel_.set_match(agreed.key);
        discovery_.close();
    } else if (!entered_) {
        entered_ = true;
        flow_.menu().level = "level1";
        flow_.menu().item = MenuItem::Wave;
        flow_.enter();
        if (!flow_.menu().error.empty()) reset(flow_.menu().error);
    }
}

void NetworkMode::exchange(uint64_t clock) {
    channel_.pump(clock);
    if (!opened_ && channel_.ready()) {
        if (!host_ && !lobby_->join(channel_.endpoint(), clock)) {
            reset(lobby_error(lobby_->refusal()));
            return;
        }
        opened_ = true;
    }
    if (!opened_ && channel_.failed()) {
        reset("Relay could not verify the room");
        return;
    }
    for (int i = 0; i < 32; ++i) {
        uint8_t bytes[udp::MAX_DATAGRAM];
        udp::Endpoint from;
        const int n = channel_.read(bytes, from);
        if (n < 0) { reset("Network socket closed"); return; }
        if (!n) break;
        const auto packet = channel_.logical(from, {bytes, static_cast<size_t>(n)});
        if (!opened_ || packet.empty()) continue;
        lobby_->receive(from, packet, clock);
        if (fight_) fight_->receive(from, packet, clock);
    }
    if (lobby_->phase() == net::LobbyPhase::Rejected) {
        reset(lobby_error(lobby_->refusal()));
        return;
    }
    if (auto tx = lobby_->poll(clock)) channel_.send(tx->to, tx->datagram, false);
    open_match(clock);
    if (!fight_) return;
    fight_->pump(clock, send_fight, &channel_);
    if (fight_->phase() == NetworkPhase::Solo) {
        channel_.close();
        solo_ = true;
    } else if (fight_->phase() == NetworkPhase::Failed) {
        reset("Match start or input failed");
    } else if (fight_->phase() == NetworkPhase::Desynced) {
        dump(clock);
    }
}

void NetworkMode::poll() {
    const auto clock = now();
    if (lookup_) finish_lookup(clock);
    discovery_.poll(clock);
    if (lobby_ && !solo_) exchange(clock);
}

void NetworkMode::advance(Hotseat& hotseat) {
    if (!fight_) {
        hotseat.engine().drain();
        return;
    }
    const auto frame = Clock::now();
    constexpr std::chrono::nanoseconds tick_time{1'000'000'000 / 60};
    if (frame < next_tick_) { effects(hotseat); return; }
    if (frame - next_tick_ > 4 * tick_time) next_tick_ = frame;
    next_tick_ += tick_time;
    Players players{};
    hotseat.tick(fight_->tick() + 1, players);
    fight_->advance(players[0], now());
    effects(hotseat);
}

void NetworkMode::effects(Hotseat& hotseat) {
    while (const auto* effect = fight_->confirmed_effect()) {
        const auto& player = effect->after.at[fight_->local_seat()];
        if (player.present) hit_feedback(effect->hits, player.body, hotseat.assignment(0));
        if (effect->hits.count || effect->knocked.count || effect->waves.count || effect->checkpoint) {
            std::printf("network confirmed tick %u hits %u knockouts %u waves %u checkpoint %u\n",
                        effect->tick, effect->hits.count, effect->knocked.count, effect->waves.count,
                        static_cast<unsigned>(effect->checkpoint));
        }
    }
}

void NetworkMode::dump(uint64_t clock) {
    status_ = dump_.write(*fight_, clock) ? "Desync. Replay and snapshot saved." : "Desync. Dump write failed.";
}

} // namespace rumble
