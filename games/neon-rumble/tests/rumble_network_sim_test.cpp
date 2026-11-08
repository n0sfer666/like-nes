#include "rumble_network_fixture.hpp"

#include "platform_args.hpp"
#include "platform_io.hpp"
#include "rumble_wave_script.hpp"

namespace {

using namespace rumble;
using namespace rumble::test;

InputTape record_input(const asset::BundleView& bundle, NetworkSettings terms) {
    InputTape tape{};
    auto play = level(bundle);
    auto sim = std::make_unique<NetworkSimulation>(*play, terms);
    std::array<WaveScript, 2> bots{};
    bots[1].seat = 1;
    for (uint32_t at = 0; at < TARGET + DELAY; ++at) {
        std::array<PlayerCommand, 2> row{};
        for (uint32_t p = 0; p < row.size(); ++p) {
            row[p].input.present = true;
            if (at >= DELAY) row[p] = bots[p].next(play->brawl, at);
        }
        if (at >= DELAY) tape[at - DELAY] = row;
        sim->step(row.data());
    }
    return tape;
}

uint64_t paired(const asset::BundleView& bundle, NetworkSettings terms, const InputTape& tape, Path path) {
    auto pair = std::make_unique<Pair>(bundle, terms, tape, path);
    pair->run();
    const auto a = pair->a.fight.hash_at(TARGET);
    const auto b = pair->b.fight.hash_at(TARGET);
    require(a && b && *a == *b, "same confirmed hash");
    require(pair->a.play->brawl.seats.at[1].present && pair->b.play->brawl.seats.at[0].present,
            "both fighters took part");
    require(pair->a.hits > 0 && pair->a.hits == pair->b.hits && pair->a.spawns > 0 &&
            pair->a.spawns == pair->b.spawns, "combat and wave effects agree");
    if (path == Path::Mixed) {
        require(pair->lost > 0 && pair->duplicated > 0 &&
                pair->a.fight.rollbacks() + pair->b.fight.rollbacks() > 0, "loss, duplicates and rollback exercised");
    }
    if (path == Path::Recover || path == Path::RecoverOneWay) {
        require(pair->a.paused && pair->b.paused && pair->a.fight.reconnects() == 1 &&
                pair->b.fight.reconnects() == 1, "both sides paused and recovered exactly once");
    }
    std::printf("rumble-network-sim: paired %d tick %u hash %016llx\n",
                static_cast<int>(path), TARGET, static_cast<unsigned long long>(*a));
    return *a;
}

void solos(const asset::BundleView& bundle, NetworkSettings terms, const InputTape& tape, Path path) {
    auto pair = std::make_unique<Pair>(bundle, terms, tape, path);
    pair->run();
    if (path == Path::SoloOneWay) require(pair->delayed_echo, "held pre-pause response delivered during pause");
    for (uint32_t p = 0; p < pair->peers.size(); ++p) {
        const auto& peer = *pair->peers[p];
        require(peer.fight.phase() == NetworkPhase::Solo && peer.paused && peer.solo_tick < TARGET &&
                peer.fight.tick() == TARGET, "each fight advanced after the pause");
        const auto gap = peer_clock(peer.solo_at, p) - peer_clock(peer.pause_at, p);
        require(gap >= NETWORK_RECONNECT_MS && gap <= NETWORK_RECONNECT_MS + 1, "30 second deadline");
        const auto& seats = peer.play->brawl.seats;
        require(seats.at[p].present && !seats.at[1 - p].present, "only the remote seat left");
        const auto hash = peer.fight.hash_at(TARGET);
        const auto live = peer.play->brawl.hash();
        require(hash.has_value(), "solo hash");
        if (pair->stale[p]) pair->inject(*pair->stale[p]);
        require(peer.fight.hash_at(TARGET) == hash && peer.play->brawl.hash() == live && peer.fight.tick() == TARGET,
                "old packets do not influence solo state");
    }
    require(pair->a.fight.hash_at(TARGET) != pair->b.fight.hash_at(TARGET), "independent solo states");
}

void settings_and_snapshot(const asset::BundleView& bundle, NetworkSettings terms) {
    terms.difficulty = Difficulty::Hard;
    terms.friendly_fire = true;
    auto play = level(bundle);
    auto sim = std::make_unique<NetworkSimulation>(*play, terms);
    require(play->brawl.hit_rules.friendly_fire && play->brawl.wave.ai.rng.state == terms.seed,
            "match applies friendly fire and seed");
    const auto* movers = play->brawl.wave.platform.movers.get();
    require(movers != nullptr && play->brawl.wave.platform.ropes.has_value(), "real movers and ropes exist");
    auto step = [&] {
        const std::array<PlayerCommand, 2> commands{input(sim->tick(), 0), input(sim->tick(), 1)};
        sim->step(commands.data());
    };
    for (int i = 0; i < 350; ++i) step();
    require(play->brawl.wave.ai.tokens.capacity == 3, "hard mode changes AI attack budget");
    NetworkSnapshot snapshot;
    sim->save(snapshot);
    for (int i = 0; i < 8; ++i) step();
    const auto expected = sim->hash_at(358);
    sim->restore(snapshot);
    require(sim->tick() == 350 && play->brawl.wave.platform.movers.get() == movers,
            "restore keeps scene time and mover world address");
    for (int i = 0; i < 8; ++i) step();
    require(expected && sim->hash_at(358) == expected, "whole play restores and replays");
}

void lost_start(const asset::BundleView& bundle, NetworkSettings terms) {
    Peer peer(bundle, terms, 0);
    const auto now = peer_clock(0, 0);
    require(!peer.fight.advance(input(0, 0), now), "no predicted step before authenticated input");
    require(!peer.fight.advance(input(0, 0), now + NETWORK_START_MS) &&
            peer.fight.phase() == NetworkPhase::Failed && peer.fight.tick() == 0,
            "missing first exchange expires");
}

void input_wire() {
    NetworkInputPacket packet;
    packet.probe = 1;
    packet.hash_count = 1;
    packet.count = 1;
    packet.inputs[0] = input(2, 0);
    std::array<uint8_t, net::SESSION_PAYLOAD> bytes{};
    const auto size = encode_network_input(packet, bytes);
    NetworkInputPacket out;
    require(size > 0 && decode_network_input({bytes.data(), size}, out) &&
            out.inputs[0] == packet.inputs[0], "input fields survive the wire");
    for (size_t n = 0; n < size; ++n) {
        require(!decode_network_input({bytes.data(), n}, out) && out.probe == packet.probe,
                "truncated input rejects atomically");
    }
    require(!decode_network_input({bytes.data(), size + 1}, out), "input has no trailing bytes");
    packet.inputs[0].input.move.move_z = fix32::from_int(2);
    require(!encode_network_input(packet, bytes), "out-of-range axis is rejected");
    packet.inputs[0] = input(2, 0);
    packet.begin = UINT32_MAX;
    require(!encode_network_input(packet, bytes), "input tick range cannot wrap");
}

bool teammate_hit(const asset::BundleView& bundle, NetworkSettings terms, bool friendly) {
    terms.friendly_fire = friendly;
    auto play = level(bundle);
    auto sim = std::make_unique<NetworkSimulation>(*play, terms);
    std::array<PlayerCommand, 2> players{};
    for (auto& player : players) player.input.present = true;
    sim->step(players.data());
    auto* host = play->brawl.pool.find(play->brawl.seats.at[0].body);
    auto* guest = play->brawl.pool.find(play->brawl.seats.at[1].body);
    require(host && guest, "friendly-fire fixture has two bodies");
    host->pos.x = fix32::from_int(300);
    host->pos.z = fix32::from_int(248);
    host->facing = 1;
    host->immune = 0;
    guest->pos.x = fix32::from_int(319);
    guest->pos.z = host->pos.z;
    guest->immune = 0;
    const auto id = guest->id;
    const auto health = guest->hp;
    players[0].attack = Attack::Punch;
    for (int i = 0; i < 20; ++i) sim->step(players.data());
    guest = play->brawl.pool.find(id);
    return guest && guest->hp < health;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    require(argc == 2, "game.bundle path");
    platform::MappedFile file;
    asset::BundleView bundle;
    require(file.open(argv[1]) && bundle.open(file.data(), file.size(), false), "validated game bundle");
    NetworkSettings terms;
    terms.game_hash = bundle.header().bundle_hash;
    terms.seed = 27;
    input_wire();
    require(teammate_hit(bundle, terms, true) && !teammate_hit(bundle, terms, false),
            "negotiated friendly fire changes real teammate damage");
    settings_and_snapshot(bundle, terms);
    lost_start(bundle, terms);
    const auto tape = record_input(bundle, terms);
    const auto clean = paired(bundle, terms, tape, Path::Clean);
    for (auto path : {Path::Mixed, Path::HighLag, Path::Recover, Path::RecoverOneWay}) {
        require(paired(bundle, terms, tape, path) == clean, "same input agrees across network faults");
    }
    solos(bundle, terms, tape, Path::Solo);
    solos(bundle, terms, tape, Path::SoloOneWay);
    std::puts("rumble-network-sim: PASS");
}
