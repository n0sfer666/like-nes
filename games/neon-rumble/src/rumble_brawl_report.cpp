#include "rumble_brawl_report.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "body_hash.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_layers.hpp"
#include "rumble_roster_quads.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;
namespace gfx = framework::graphics;

const char* side(const br::Body& b) { return b.facing < 0 ? "left" : "right"; }

void report_floor(const br::DepthFloor& f) {
    std::printf("neon-rumble: depth band %d..%d x %d..%d, %u wall(s)", f.band.x0.to_int(), f.band.x1.to_int(),
                f.band.z0.to_int(), f.band.z1.to_int(), f.wall_count);
    for (uint32_t i = 0; i < f.wall_count; ++i) {
        const br::FloorRect& w = f.walls[i];
        std::printf(" %d..%d x %d..%d", w.x0.to_int(), w.x1.to_int(), w.z0.to_int(), w.z1.to_int());
    }
    std::printf("\n");
}

} // namespace

void report_fighters(const Level& level, const Fighters& fighters, const Brawl& brawl) {
    std::printf("neon-rumble: clips %u in the table\n", fighters[0].clips.count());
    for (uint32_t i = 0; i < FIGHTERS; ++i) {
        const br::Body& b = brawl.body(i);
        std::printf("neon-rumble: fighter %s sheet %ux%u, body %d,%d facing %s\n", fighters[i].name,
                    fighters[i].sheet.width, fighters[i].sheet.height, b.pos.x.to_int(), b.pos.z.to_int(), side(b));
    }
    report_floor(brawl.floor);
    Layers layers;
    FighterQuads quads;
    const framework::Vec2 follow = screen_plane(brawl.body(0));
    LayerStats st = layers.build(level, gfx::viewport_fit({960, 540}), 0, &follow);
    for (const uint32_t i : brawl.draw_order()) {
        const br::Body& b = brawl.body(i);
        const Pose p = fighters[i].pose(b);
        const FighterStats fs = quads.add(fighters[i], p, screen_plane(b), depth_overlay(brawl, i), layers, st,
                                          sheet_texture(level.texture_count, i), solid_texture(level.texture_count),
                                          true);
        std::printf("neon-rumble: pose tick 0: %s frame %u flip %d, %zu hit, %zu hurt, %zu push, "
                    "%u overlay quad(s), %u rejected, %u dropped\n",
                    p.name, p.frame, p.flip ? 1 : 0, gfx::frame_boxes(p.clip, p.frame, gfx::BoxKind::Hit).size(),
                    gfx::frame_boxes(p.clip, p.frame, gfx::BoxKind::Hurt).size(),
                    gfx::frame_boxes(p.clip, p.frame, gfx::BoxKind::Push).size(), fs.overlay, fs.rejected,
                    fs.dropped + st.dropped);
    }
}

void report_brawl(const Fighters& fighters, const Brawl& brawl, uint32_t tick) {
    std::printf("neon-rumble: brawl tick %u: hash %016llx,", tick,
                static_cast<unsigned long long>(br::state_hash(brawl.pool)));
    for (uint32_t i = 0; i < FIGHTERS; ++i) {
        const br::DepthBody& d = brawl.body(i).pos;
        std::printf(" %s %d,%d y %d hp %d,", fighters[i].name, d.x.to_int(), d.z.to_int(), d.y.to_int(),
                    brawl.body(i).hp);
    }
    std::printf(" draw");
    for (const uint32_t i : brawl.draw_order()) std::printf(" %s", fighters[i].name);
    std::printf("\n");
}

void report_hits(const Brawl& brawl, uint32_t tick) {
    std::array<int64_t, FIGHTERS> hp{};
    for (uint32_t i = 0; i < FIGHTERS; ++i) hp[i] = brawl.hp_before[i];
    for (uint32_t i = 0; i < brawl.events.count; ++i) {
        const br::HitEvent& e = brawl.events.at[i];
        const uint32_t a = brawl.fighter_of(e.attacker);
        const uint32_t t = brawl.fighter_of(e.target);
        if (a == FIGHTERS || t == FIGHTERS || !brawl.body(a).struck.has(e.target, e.box)) continue;
        const br::MoveSlot& m = brawl.kinds.types[e.kind].moves[e.move];
        hp[t] = std::max<int64_t>(hp[t] - m.strike.damage, 0);
        std::printf("neon-rumble: hit tick %u: %s -> %s, damage %u, hp %lld\n", tick,
                    brawl.kinds.types[e.kind].clips->name(m.clip), ROSTER[t].fighter, m.strike.damage,
                    static_cast<long long>(hp[t]));
    }
    if (brawl.events.dropped > 0)
        std::printf("neon-rumble: hit tick %u: %u event(s) dropped\n", tick, brawl.events.dropped);
    if (brawl.events.count + brawl.events.dropped > 0) std::fflush(stdout);
}

} // namespace rumble
