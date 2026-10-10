#pragma once
#include <array>
#include <cstdint>

#include "ai_step.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace framework::ai::test {

using brawl::Body;
using brawl::Command;

// Игрушечный ИИ гейта: подойти, взять жетон или держать кольцо, ударить, пока жетон не истёк. Каждое решение тянет из
// ГСЧ мира, цель липкая (выбирается при входе в подход), таймеры живут в мозге — так забытое в
// снапшоте любое из них меняет дальнейший бой, а не только хеш (гейт судит по хешу мира без ai_hash).
enum State : uint8_t { APPROACH, STRIKE, HOLD, STATES };

inline uint16_t jab_strike(const brawl::test::Arena& arena) { return brawl::test::strike(arena, arena.jab).strike; }

inline const brawl::test::Arena& arena() {
    static const brawl::test::Arena a;
    return a;
}

inline int32_t sign(fix32 v) { return v.raw > 0 ? 1 : (v.raw < 0 ? -1 : 0); }

inline brawl::EntId nearest_player(const Mind& m) {
    brawl::EntId best;
    int64_t best_d = INT64_MAX;
    for (uint32_t i = 0; i < m.pool.count; ++i) {
        const Body& p = m.pool.bodies[i];
        if (p.team != 0) continue;
        const int64_t dx = int64_t{p.pos.x.raw} - m.body.pos.x.raw, dz = int64_t{p.pos.z.raw} - m.body.pos.z.raw;
        const int64_t d = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
        if (d >= best_d) continue;
        best_d = d;
        best = p.id;
    }
    return best;
}

inline void walk(Command& out, int32_t mx, int32_t mz) {
    out.input.present = true;
    out.input.move.move_x = fix32::from_int(mx);
    out.input.move.move_z = fix32::from_int(mz);
}

inline void enter(Mind& m, State s, uint16_t timer) {
    m.brain.state = s;
    m.brain.timer = timer;
}

inline void approach(Mind& m, Command& out) {
    if (m.brain.target.seq == 0) m.brain.target = nearest_player(m);
    const Body* t = m.pool.find(m.brain.target);
    if (t == nullptr) return;
    const fix32 dx = t->pos.x - m.body.pos.x;
    if ((dx.raw < 0 ? -dx.raw : dx.raw) > fix32::from_int(48).raw)
        return walk(out, below(m.rng, 4) == 0 ? 0 : sign(dx), sign(t->pos.z - m.body.pos.z));
    if (try_take_token(m.tokens, m.body.id, m.tick)) return enter(m, STRIKE, 60);
    enter(m, HOLD, static_cast<uint16_t>(8 + below(m.rng, 16)));
}

inline void strike(Mind& m, Command& out) {
    const Body* t = m.pool.find(m.brain.target);
    if (t == nullptr || m.brain.timer == 0 || !holds_token(m.tokens, m.body.id) ||
        m.body.react != brawl::Reaction::None) {
        release_token(m.tokens, m.body.id);
        m.brain.target = brawl::EntId{};
        return enter(m, APPROACH, 0);
    }
    const fix32 dx = t->pos.x - m.body.pos.x, dz = t->pos.z - m.body.pos.z;
    const bool near = (dx.raw < 0 ? -dx.raw : dx.raw) <= fix32::from_int(16).raw &&
                      (dz.raw < 0 ? -dz.raw : dz.raw) <= fix32::from_int(2).raw;
    if (!near) return walk(out, sign(dx), sign(dz));
    walk(out, 0, 0);
    out.strike = jab_strike(arena());
}

inline void hold(Mind& m, Command& out) {
    walk(out, 0, static_cast<int32_t>(below(m.rng, 3)) - 1);
    if (m.brain.timer == 0) enter(m, APPROACH, 0);
}

inline constexpr std::array<Think, STATES> THINK{approach, strike, hold};

} // namespace framework::ai::test
