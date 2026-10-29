#pragma once
#include <cstdint>
#include <vector>

#include "body_hash.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "framework_physics_pendulum_energy.hpp"
#include "hash_mix.hpp"
#include "rope_swing.hpp"
#include "stage_movers.hpp"
#include "stage_ropes.hpp"
#include "stage_ropes_hash.hpp"
#include "stage_sections_hash.hpp"

namespace framework::brawl::test {

constexpr int32_t PIVOT_X = 600;
constexpr int32_t PIVOT_Y = 60;
constexpr int32_t LENGTH = 96;

inline scene::Rope rope(int32_t x, int32_t y, int32_t length) {
    return scene::Rope{{nullptr, {at(x), at(y)}}, at(length)};
}

inline fix32 deflection(fix32 length, int32_t degrees) { return physics::pendulum_cap(length) * at(degrees) / at(90); }

using physics::test::EnergyGate;

inline int64_t rope_energy(const RopeState& r, fix32 length, fix32 g) {
    return physics::test::pendulum_energy(r.swing, length, g);
}

// Трос в platform-секции фикстуры `Sections`: опора (600, 60), длина 96 — в горизонтали конец
// проходит x [504, 696] внутри секции, в отвесе корпус седока висит на 52 px над полом.
struct Swing {
    Sections w;
    StageRopes ropes;
    StageMovers* movers = nullptr;

    explicit Swing(const std::vector<Solid>& solids = {},
                   const std::vector<scene::Rope>& list = {rope(PIVOT_X, PIVOT_Y, LENGTH)})
        : w(solids), ropes(level(w, list)) {}

    static const scene::LevelObjects& level(Sections& w, const std::vector<scene::Rope>& list) {
        w.s.level.ropes = list;
        return w.s.level;
    }

    void step(const std::vector<Command>& c) {
        HitEvents events;
        step_stage(w.s.pool, c, w.world(), w.s.camera, w.s.seats, w.s.sections, events, movers, &ropes);
    }

    void idle(uint32_t ticks) {
        for (uint32_t t = 0; t < ticks; ++t) step({});
    }

    EntId fighter(int32_t x) {
        const EntId id = w.body(x);
        step({});
        return id;
    }

    SectionBody& section(EntId id) { return *w.s.sections.find(id); }

    fix32 g() const { return w.platform.profile.gravity_fall * w.platform.dt * w.platform.dt; }

    Vec2 end(uint32_t i = 0) const {
        const scene::Rope& r = w.s.level.ropes[i];
        return physics::pendulum_end(r.mark.at, r.length, ropes.at[i].swing.arc);
    }

    Vec2 hands(EntId id) {
        const Vec2 c = section(id).c.position;
        return {c.x, c.y - hull_half(w.platform).y};
    }

    // Корпус в воздухе, неподвижный, руки — в `at`.
    void hang_hands(EntId id, Vec2 to) {
        SectionBody& s = section(id);
        s.c.position = {to.x, to.y + hull_half(w.platform).y};
        s.c.velocity = {};
        s.c.state = character::MoveState::Air;
        s.c.on_ground = false;
        mirror_body(s.c, hull_foot(w.platform), w.platform.dt, w.get(id).pos);
    }

    uint64_t hash() const {
        uint64_t h = state_hash(w.s.pool);
        physics::mix_u64(h, sections_hash(w.s.sections));
        physics::mix_u64(h, ropes_hash(ropes));
        return h;
    }
};

inline Command grab(int32_t dir = 0) {
    Command c;
    c.input = walk(dir);
    c.input.buttons = button::GRAB;
    return c;
}

inline Command leap() {
    Command c = grab();
    c.input.buttons = button::GRAB | button::JUMP;
    return c;
}

} // namespace framework::brawl::test
