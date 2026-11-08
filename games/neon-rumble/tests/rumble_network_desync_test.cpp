#include "rumble_network_fixture.hpp"

#include "platform_fs.hpp"
#include "rumble_net_dump.hpp"

bool rumble::test::Pair::drop_desync(const net::Datagram& packet) {
    const auto inner = net::unwrap_session(match({}, 1).key, ENDPOINTS[0], {packet.bytes, packet.size});
    if (inner.size() >= 4 && std::memcmp(inner.data(), "DIFF", 4) == 0) {
        if (!notice_block_until) notice_block_until = now + 6'000;
        return now < notice_block_until;
    }
    NetworkInputPacket input;
    return inner.size() > net::HEADER_BYTES &&
        decode_network_input(inner.subspan(net::HEADER_BYTES), input) &&
        input.hash_count && input.hash_tick + input.hash_count - 1 > hide_host_hashes_after;
}

void test_network_desync(const asset::BundleView& bundle, const rumble::test::InputTape& tape,
                         const std::string& dir) {
    using namespace rumble;
    using namespace rumble::test;
    NetworkSettings settings;
    settings.game_hash = bundle.header().bundle_hash;
    settings.seed = 27;
    auto pair = std::make_unique<Pair>(bundle, settings, tape, Path::Clean);
    pair->desync_case = true;
    bool injected = false;
    for (; pair->now < 10'000; ++pair->now) {
        pair->step();
        auto& fight = pair->a.fight;
        if (!injected && fight.tick() >= 80 && fight.tick() == fight.confirmed()) {
            auto& body = pair->a.play->brawl.pool.bodies[0];
            require(body.hp > 1, "desync mutation changes a real fighter");
            --body.hp;
            pair->hide_host_hashes_after = fight.tick();
            injected = true;
        }
        if (pair->a.fight.mismatch() && pair->b.fight.mismatch()) break;
    }
    require(injected && pair->a.fight.mismatch() && pair->a.fight.mismatch() == pair->b.fight.mismatch(),
            "both peers froze at the first common mismatch");
    require(pair->notice_block_until && pair->now >= pair->notice_block_until && pair->b.paused,
            "paused guest learned the mismatch after a six-second one-way outage");
    require(platform::ensure_dir(dir) &&
            write_network_dump(pair->a.fight, dir + "/host") &&
            write_network_dump(pair->b.fight, dir + "/guest"), "both peers wrote replay and snapshot");
    std::printf("rumble-network-desync: PASS tick %u\n", *pair->a.fight.mismatch());
}
