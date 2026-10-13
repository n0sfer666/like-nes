#pragma once
#include <cstdint>
#include <vector>

#include "body_hash.hpp"
#include "fixtrig.hpp"
#include "framework_brawl_section_fixture.hpp"
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

inline fix32 deflection(fix32 length, int32_t degrees) { return rope_cap(length) * at(degrees) / at(90); }

// Теневая энергия полунеявного Эйлера, px²/тик² в Q16.16 без сдвига: `½v² + gL(1 − cos θ) − ½·v·g·sin θ`.
// Её шаг сохраняет точно, а не `½v² + gL(1 − cos θ)`: та колеблется на полшага каждый тик и судила
// бы о росте по колебанию, а не по росту.
inline int64_t rope_energy(const RopeState& r, fix32 length, fix32 g) {
    const fix32 t = rope_turns(r.arc, length);
    const int64_t v = r.speed.raw;
    const int64_t kinetic = v * v / 2;
    const int64_t potential = static_cast<int64_t>((g * length).raw) * (at(1) - cos_turns(t)).raw;
    const int64_t shadow = v * (g * sin_turns(t)).raw / 2;
    return kinetic + potential - shadow;
}

// Гейт энергии: пик каждого окна из `WINDOW` тиков не выше пика прошлого, пик последнего полного
// окна ниже энергии начала. Окно, а не тик: энергия на тике колеблется в младших разрядах и на честном шаге.
// Пик первого окна с началом не сравнивается: дуга в угол округляется, и первые тики выходят над
// ним на доли процента (замер — до 0.26 % на 256 px и 5°), а не на рост от шага к шагу.
struct EnergyGate {
    static constexpr uint32_t WINDOW = 500;
    int64_t start;
    int64_t last_peak;
    int64_t peak;
    uint32_t tick = 0;
    bool rose = false;

    explicit EnergyGate(int64_t e) : start(e), last_peak(INT64_MAX), peak(e) {}

    void add(int64_t e) {
        if (++tick % WINDOW == 0) {
            rose = rose || last_peak < peak;
            last_peak = peak;
            peak = e;
        }
        if (peak < e) peak = e;
    }

    bool settles() const { return !rose && last_peak < start; }
};

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
        return rope_end(r.mark.at, r.length, ropes.at[i].arc);
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
