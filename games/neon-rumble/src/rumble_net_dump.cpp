#include "rumble_net_dump.hpp"
#include "rumble_net_dump_fields.hpp"

#include <chrono>
#include <cstdio>

#include "platform_fs.hpp"
#include "save.hpp"

namespace rumble {
namespace {

void header(DumpFields out, const NetworkFight& fight, uint32_t tick) {
    out.put("dump_version", NETWORK_DUMP_VERSION);
    out.put("protocol", net::MATCH_VERSION);
    out.put("match_hash", fight.match_hash());
    out.put("tick", tick);
}

void command(DumpFields out, const PlayerCommand& c) {
    out.put("present", c.input.present);
    out.put("x", c.input.move.move_x);
    out.put("z", c.input.move.move_z);
    out.put("jump", c.input.move.jump_held);
    out.put("up", c.input.move.up_held);
    out.put("down", c.input.move.down_held);
    out.put("buttons", c.input.buttons);
    out.put("attack", c.attack);
}

bool store(const std::string& path, const std::string& body) {
    return platform::write_atomic(path,
        {reinterpret_cast<const uint8_t*>(body.data()), body.size()});
}

} // namespace

bool write_network_dump(const NetworkFight& fight, const std::string& stem) {
    const auto at = fight.mismatch();
    if (!at || stem.empty()) return false;
    NetworkSnapshot world;
    if (!fight.capture(*at, world)) return false;

    std::string replay, snapshot;
    DumpFields rows(replay), fields(snapshot);
    header(rows, fight, *at);
    header(fields, fight, *at);
    uint32_t first = *at;
    while (first > 0 && *at - first < NETWORK_HISTORY && fight.replay_at(first - 1)) --first;
    rows.put("begin", first);
    for (uint32_t tick = first; tick <= *at; ++tick) {
        const auto* frame = fight.replay_at(tick);
        if (!frame) return false;
        const auto row = rows.child("frames").at(tick);
        row.put("hash", frame->hash);
        for (uint32_t p = 0; p < br::SEATS; ++p) command(row.child("player").at(p), frame->inputs[p]);
    }
    const auto hash = fight.hash_at(*at);
    if (!hash) return false;
    fields.put("state_hash", *hash);
    dump_world(world, fields.child("world"));
    return store(stem + ".replay", replay) && store(stem + ".snapshot", snapshot);
}

bool NetworkDump::write(const NetworkFight& fight, uint64_t now) {
    if (!fight.mismatch()) return false;
    if (saved_ == fight.mismatch()) return true;
    if (now < next_) return false;
    next_ = now + 1'000;
    if (stem_.empty()) {
        save::init("neon-rumble");
        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        const auto name = "network-" + std::to_string(stamp) + "-" + std::to_string(fight.local_seat());
        stem_ = save::file_path(name);
    }
    if (!write_network_dump(fight, stem_)) return false;
    saved_ = fight.mismatch();
    std::printf("network dump %s tick %u\n", stem_.c_str(), *saved_);
    return true;
}

} // namespace rumble
