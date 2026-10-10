#include "rumble_brawl_report.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "body_guard.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_layers.hpp"
#include "rumble_roster_quads.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;
namespace gfx = framework::graphics;

const char* side(const br::Body& b) { return b.facing < 0 ? "left" : "right"; }

const char* reaction_name(br::Reaction r) {
    switch (r) {
    case br::Reaction::Hurt: return "hurt";
    case br::Reaction::Fall: return "fall";
    case br::Reaction::Down: return "down";
    case br::Reaction::Getup: return "getup";
    case br::Reaction::Block: return "block";
    case br::Reaction::Dodge: return "dodge";
    case br::Reaction::Thrown: return "thrown";
    case br::Reaction::None: break;
    }
    return "stand";
}

// В режиме волны тел одного вида несколько, и метка несёт seq; вне волны строки прежние.
struct Label {
    std::array<char, 32> text{};
};

Label label(const Brawl& brawl, br::EntId id, uint8_t kind) {
    Label l;
    const char* name = kind < FIGHTERS ? ROSTER[kind].fighter : "?";
    if (brawl.wave.on) std::snprintf(l.text.data(), l.text.size(), "%s#%u", name, id.seq);
    else std::snprintf(l.text.data(), l.text.size(), "%s", name);
    return l;
}

Label label(const Brawl& brawl, const br::Body& b) { return label(brawl, b.id, b.kind); }

uint32_t slot_of(const br::BodyPool& pool, br::EntId id) {
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.bodies[i].id == id) return i;
    return pool.count;
}

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
        const br::Body& b = brawl.spawns[i];
        std::printf("neon-rumble: fighter %s sheet %ux%u, body %d,%d facing %s\n", fighters[i].name,
                    fighters[i].sheet.width, fighters[i].sheet.height, b.pos.x.to_int(), b.pos.z.to_int(), side(b));
    }
    report_floor(brawl.floor);
    Layers layers;
    FighterQuads quads;
    const framework::Vec2 follow = screen_plane(brawl.spawns[PLAYER]);
    LayerStats st = layers.build(level, gfx::viewport_fit({960, 540}), 0, &follow);
    for (const uint32_t i : brawl.draw_order()) {
        const br::Body& b = brawl.pool.bodies[i];
        const Pose p = fighters[b.kind].pose(b);
        const FighterStats fs = quads.add(fighters[b.kind], p, screen_plane(b), depth_overlay(brawl, b), layers, st,
                                          sheet_texture(level.texture_count, b.kind),
                                          solid_texture(level.texture_count), true);
        std::printf("neon-rumble: pose tick 0: %s frame %u flip %d, %zu hit, %zu hurt, %zu push, "
                    "%u overlay quad(s), %u rejected, %u dropped\n",
                    p.name, p.frame, p.flip ? 1 : 0, gfx::frame_boxes(p.clip, p.frame, gfx::BoxKind::Hit).size(),
                    gfx::frame_boxes(p.clip, p.frame, gfx::BoxKind::Hurt).size(),
                    gfx::frame_boxes(p.clip, p.frame, gfx::BoxKind::Push).size(), fs.overlay, fs.rejected,
                    fs.dropped + st.dropped);
    }
}

// Тела по виду, а внутри вида по слоту: P2, вошедший позже манекена, стоит в строке на своём месте.
void report_brawl(const Brawl& brawl, uint32_t tick) {
    std::printf("neon-rumble: brawl tick %u: hash %016llx,", tick,
                static_cast<unsigned long long>(brawl.hash()));
    for (uint32_t k = 0; k < FIGHTERS; ++k)
        for (uint32_t i = 0; i < brawl.pool.count; ++i) {
            const br::Body& b = brawl.pool.bodies[i];
            if (b.kind != k) continue;
            std::printf(" %s %d,%d y %d hp %d,", label(brawl, b).text.data(), b.pos.x.to_int(), b.pos.z.to_int(),
                        b.pos.y.to_int(), b.hp);
        }
    std::printf(" draw");
    for (const uint32_t i : brawl.draw_order()) std::printf(" %s", label(brawl, brawl.pool.bodies[i]).text.data());
    std::printf("\n");
}

void report_hits(const Brawl& brawl, uint32_t tick) {
    const br::BodyPool& pool = brawl.pool;
    std::array<int64_t, br::POOL_CAPACITY> hp{};
    for (uint32_t i = 0; i < pool.count; ++i) hp[i] = brawl.hp_before[i];
    for (uint32_t i = 0; i < brawl.events.count; ++i) {
        const br::HitEvent& e = brawl.events.at[i];
        const uint32_t a = slot_of(pool, e.attacker);
        const uint32_t t = slot_of(pool, e.target);
        if (a == pool.count || t == pool.count) continue;
        if (!e.kept) continue;
        const br::Body& by = pool.bodies[a];
        const br::Body& to = pool.bodies[t];
        const br::MoveSlot& m = brawl.kinds.types[e.kind].moves[e.move];
        // Удар в блок hp не снимает. Судим по телу после шага: блок от принятого удара не спадает.
        const uint32_t damage = br::guards(to, by, m.strike) ? 0 : m.strike.damage;
        hp[t] = std::max<int64_t>(hp[t] - damage, 0);
        std::printf("neon-rumble: hit tick %u: %s/%s -> %s, damage %u, hp %lld\n", tick, label(brawl, by).text.data(),
                    m.name, label(brawl, to).text.data(), damage, static_cast<long long>(hp[t]));
    }
    if (brawl.events.dropped > 0)
        std::printf("neon-rumble: hit tick %u: %u event(s) dropped\n", tick, brawl.events.dropped);
    if (brawl.events.count + brawl.events.dropped > 0) std::fflush(stdout);
}

void report_reactions(const Brawl& brawl, uint32_t tick) {
    for (uint32_t i = 0; i < brawl.pool.count; ++i) {
        const br::Body& b = brawl.pool.bodies[i];
        if (b.react == brawl.react_before[i]) continue;
        std::printf("neon-rumble: react tick %u: %s %s\n", tick, label(brawl, b).text.data(), reaction_name(b.react));
        std::fflush(stdout);
    }
}

void report_wave(const Brawl& brawl, uint32_t tick) {
    if (!brawl.wave.on) return;
    for (uint32_t i = 0; i < brawl.knocked.count; ++i) {
        const Knockout& k = brawl.knocked.at[i];
        std::printf("neon-rumble: knockout tick %u: %s", tick, label(brawl, k.body, k.kind).text.data());
        const br::Body* back = k.kind < br::SEATS ? brawl.pool.find(brawl.seats.at[k.kind].body) : nullptr;
        if (back != nullptr) std::printf(", back as %s", label(brawl, *back).text.data());
        std::printf("\n");
    }
    if (brawl.wave.cleared) std::printf("neon-rumble: wave tick %u: wave %u cleared\n", tick, brawl.wave.number);
    if (brawl.wave.spawned) {
        std::printf("neon-rumble: wave tick %u: wave %u,", tick, brawl.wave.number);
        for (uint32_t i = 0; i < brawl.pool.count; ++i)
            if (brawl.pool.bodies[i].team != 0) std::printf(" %s", label(brawl, brawl.pool.bodies[i]).text.data());
        std::printf("\n");
    }
    if (brawl.knocked.count > 0 || brawl.wave.cleared || brawl.wave.spawned) std::fflush(stdout);
}

void report_step(const Brawl& brawl, uint32_t tick) {
    report_wave(brawl, tick);
    report_hits(brawl, tick);
    report_reactions(brawl, tick);
}

} // namespace rumble
