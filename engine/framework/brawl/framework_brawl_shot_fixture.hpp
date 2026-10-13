#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "body_kind.hpp"
#include "clipless_kind.hpp"
#include "death_step.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "projectile.hpp"

namespace framework::brawl::test {

constexpr uint8_t FIGHTER = 0;
constexpr uint8_t SHOT = 1;
constexpr uint8_t CRATE = 2;
constexpr uint8_t CLIP_SHOT = 3;
constexpr int32_t LANE = 250;
constexpr graphics::Rect16 SHOT_BOX{-4, -12, 8, 4};
constexpr graphics::Rect16 CRATE_BOX{-8, -24, 16, 24};
constexpr graphics::Rect16 HUGE_BOX{-400, -200, 800, 400};

inline Strike shot_strike() {
    Strike s;
    s.box = 3;
    s.type = HitType::Light;
    s.damage = 10;
    s.depth = at(32);
    s.hitstop = 3;
    s.hitstun = 8;
    s.knock_x = at(1);
    return s;
}

inline DepthBody aim(int32_t x, int32_t vx, int32_t z = LANE, int32_t y = 0) {
    return DepthBody{at(x), at(z), at(y), at(vx), fix32{}, fix32{}};
}

// Виды поверх уровня `Sections`: боец арены, снаряд и ящик без клипа, снаряд с клипом бойца, чей
// `box` огромен и обязан молчать — бокс берётся из кадра.
struct Shots {
    Sections sec;
    std::array<Archetype, 4> kinds{};
    bool friendly_fire = false;
    StageMovers* movers = nullptr;
    HitEvents events;
    Living living;
    DeathFacts deaths;

    explicit Shots(const std::vector<Solid>& solids = {}) : sec(solids) {
        kinds[FIGHTER] = sec.s.arena.kinds[0];
        projectile_kind(SHOT_BOX, shot_strike(), kinds[SHOT]);
        prop_kind(CRATE_BOX, at(4), kinds[CRATE]);
        kinds[CLIP_SHOT] = kinds[FIGHTER];
        kinds[CLIP_SHOT].role = Role::Projectile;
        kinds[CLIP_SHOT].box = graphics::ClipBox{static_cast<uint8_t>(graphics::BoxKind::Hit), 0, 0, HUGE_BOX};
    }

    BrawlWorld world() const {
        BrawlWorld w = sec.world();
        w.kinds = kinds;
        w.rules.friendly_fire = friendly_fire;
        return w;
    }

    BodyPool& pool() { return sec.s.pool; }
    Body* find(EntId id) { return sec.s.pool.find(id); }
    int32_t hp(EntId id) { return find(id)->hp; }
    bool flying(EntId id) { return find(id) != nullptr && find(id)->life > 0; }

    EntId fighter(int32_t x, uint8_t team, int32_t z = LANE, int8_t facing = 1) {
        return pool().spawn(dummy(x, z, facing, team, kinds[FIGHTER]));
    }

    EntId crate(int32_t x, int32_t hp, int32_t z = LANE, uint8_t team = 2) {
        Body b;
        b.pos.x = at(x);
        b.pos.z = at(z);
        b.team = team;
        b.hp = hp;
        b.kind = CRATE;
        return pool().spawn(b);
    }

    EntId shoot(EntId owner, const DepthBody& from, uint16_t life, uint8_t kind = SHOT) {
        return spawn_projectile(pool(), &sec.s.sections, owner, Shot{kind, from, life}, world());
    }

    void step(const std::vector<Command>& commands = {}) {
        events = HitEvents{};
        drop_spent(pool(), kinds, &sec.s.sections);
        note_living(pool(), living);
        step_stage(pool(), commands, world(), sec.s.camera, sec.s.seats, sec.s.sections, events, movers);
        step_deaths(pool(), living, deaths);
    }

    uint32_t lifetime(EntId id, uint32_t most = 40) {
        uint32_t t = 0;
        while (flying(id) && t < most) {
            step();
            ++t;
        }
        return t;
    }
};

} // namespace framework::brawl::test
