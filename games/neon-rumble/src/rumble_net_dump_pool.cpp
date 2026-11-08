#include "rumble_net_dump_fields.hpp"

namespace rumble {
namespace {

void dump_body(const br::Body& b, DumpFields f) {
    f.put("id", b.id.seq);
    f.put("x", b.pos.x);
    f.put("y", b.pos.y);
    f.put("z", b.pos.z);
    f.put("vx", b.pos.vx);
    f.put("vy", b.pos.vy);
    f.put("vz", b.pos.vz);
    f.put("facing", b.facing);
    f.put("team", b.team);
    f.put("owner", b.owner.seq);
    f.put("hp", b.hp);
    f.put("crushed", b.crushed);
    f.put("age", b.age);
    f.put("life", b.life);
    f.put("kind", b.kind);
    f.put("clip", b.clip);
    f.put("move", b.move);
    f.put("elapsed", b.elapsed);
    f.put("hitstop", b.hitstop);
    f.put("immune", b.immune);
    f.put("react", b.react);
    f.put("react_ticks", b.react_ticks);
    f.put("struck_count", b.struck.count);
    for (size_t i = 0; i < b.struck.at.size(); ++i) {
        const auto hit = f.child("struck").at(i);
        hit.put("seq", b.struck.at[i].seq);
        hit.put("box", b.struck.at[i].box);
    }
    f.put("chain", b.chain);
    f.put("queued_action", b.queued.action);
    f.put("queued_ticks", b.queued.ticks);
    f.put("run_dir", b.run.dir);
    f.put("run_held", b.run.held);
    f.put("run_tap", b.run.tap);
    f.put("run_tap_ticks", b.run.tap_ticks);
    f.put("grip_by", b.grip.by.seq);
    f.put("grip_team", b.grip.team);
    f.put("grip_kind", b.grip.kind);
}

} // namespace

void dump_pool(const br::BodyPool& pool, DumpFields out) {
    out.put("count", pool.count);
    out.put("next_seq", pool.next_seq);
    for (uint32_t i = 0; i < pool.count; ++i) dump_body(pool.bodies[i], out.at(i));
}

} // namespace rumble
