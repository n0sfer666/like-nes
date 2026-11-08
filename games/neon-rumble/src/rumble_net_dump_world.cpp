#include "rumble_net_dump_fields.hpp"

namespace rumble {
namespace {

void vector(DumpFields out, framework::Vec2 v) {
    out.put("x", v.x);
    out.put("y", v.y);
}

void ai_fields(const framework::ai::AiState& ai, DumpFields out) {
    out.put("rng", ai.rng.state);
    out.put("brain_count", ai.brains.count);
    for (uint32_t i = 0; i < ai.brains.count; ++i) {
        const auto& b = ai.brains.at[i];
        auto f = out.child("brain").at(i);
        f.put("body", b.body.seq);
        f.put("state", b.state);
        f.put("timer", b.timer);
        f.put("target", b.target.seq);
    }
    const auto& tokens = ai.tokens;
    out.put("token_count", tokens.count);
    out.put("token_capacity", tokens.capacity);
    out.put("token_timeout", tokens.timeout);
    for (uint32_t i = 0; i < tokens.count; ++i) {
        auto f = out.child("token").at(i);
        f.put("body", tokens.held[i].body.seq);
        f.put("taken", tokens.held[i].taken);
    }
}

void section_fields(const br::StageSections& sections, DumpFields out) {
    out.put("count", sections.count);
    for (uint32_t i = 0; i < sections.count; ++i) {
        const auto& body = sections.at[i];
        const auto& c = body.c;
        auto f = out.at(i);
        f.put("body", body.id.seq);
        vector(f.child("position"), c.position);
        vector(f.child("velocity"), c.velocity);
        f.put("state", c.state);
        f.put("coyote_left", c.coyote_left);
        f.put("buffer_left", c.buffer_left);
        f.put("ladder_regrab_left", c.ladder_regrab_left);
        f.put("on_ground", c.on_ground);
        f.put("jump_active", c.jump_active);
        f.put("jump_was_held", c.jump_was_held);
        f.put("hit_ceiling", c.hit_ceiling);
        f.put("hit_wall", c.hit_wall);
        f.put("support", c.support.index);
        f.put("crushed", c.crushed);
    }
}

void mover_fields(const framework::kinematic::MoversSnapshot& movers, DumpFields out) {
    out.put("present", movers.world.taken());
    vector(out.child("gravity"), movers.world.gravity());
    const auto& bodies = movers.world.bodies();
    out.put("body_count", bodies.size());
    for (const auto& b : bodies) {
        const auto f = out.child("body").at(b.key);
        f.put("type", b.type);
        vector(f.child("position"), b.position);
        vector(f.child("velocity"), b.velocity);
        f.put("angle", b.angle);
        f.put("angular_velocity", b.angular_velocity);
        f.put("inv_mass", b.inv_mass);
        f.put("unit_inertia", b.unit_inertia);
        f.put("max_angular", b.max_angular);
        f.put("linear_damping", b.linear_damping);
        f.put("angular_damping", b.angular_damping);
        f.put("restitution", b.material.restitution);
        f.put("friction", b.material.friction);
        f.put("layer", b.layer);
        f.put("mask", b.mask);
        f.put("trigger", b.trigger);
        f.put("shape_radius", b.shape.radius);
        f.put("shape_count", b.shape.count);
        for (uint32_t i = 0; i < b.shape.count; ++i) vector(f.child("shape").at(i), b.shape.points[i]);
    }
    out.put("follow_count", movers.follow.size());
    for (size_t i = 0; i < movers.follow.size(); ++i) {
        const auto f = out.child("follow").at(i);
        f.put("segment", movers.follow[i].segment);
        f.put("direction", movers.follow[i].direction);
        f.put("traveled", movers.follow[i].traveled);
    }
}

} // namespace

void dump_world(const NetworkSnapshot& s, DumpFields out) {
    out.put("scene_tick", s.tick);
    out.put("wave_tick", s.wave_tick);
    dump_pool(s.pool, out.child("pool"));
    for (uint32_t p = 0; p < br::SEATS; ++p) {
        const auto seat = out.child("seat").at(p);
        seat.put("present", s.seats.at[p].present);
        seat.put("body", s.seats.at[p].body.seq);
    }
    ai_fields(s.ai, out.child("ai"));
    auto camera = out.child("camera");
    vector(camera.child("center"), s.camera.center);
    camera.put("arena", s.camera.arena);
    camera.put("scroll", s.camera.autoscroll.scroll);
    camera.put("ride", s.camera.autoscroll.ride);
    camera.put("riding", s.camera.autoscroll.riding);
    vector(camera.child("settle"), s.camera.autoscroll.settle);
    section_fields(s.sections, out.child("sections"));
    const auto waves = out.child("waves");
    waves.put("cleared", s.waves.cleared);
    waves.put("active", s.waves.wave);
    waves.put("lull", s.waves.lull);
    waves.put("seen", s.waves.seen);
    out.put("checkpoint", s.progress.checkpoint);
    mover_fields(s.movers, out.child("movers"));
    const auto ropes = out.child("ropes");
    ropes.put("present", s.ropes.has_value());
    if (s.ropes) {
        ropes.put("count", s.ropes->count);
        for (uint32_t i = 0; i < s.ropes->count; ++i) {
            const auto& r = s.ropes->at[i];
            const auto f = ropes.at(i);
            f.put("arc", r.swing.arc);
            f.put("speed", r.swing.speed);
            f.put("rider", r.rider.seq);
            f.put("cooldown", r.cooldown);
        }
    }
}

} // namespace rumble
