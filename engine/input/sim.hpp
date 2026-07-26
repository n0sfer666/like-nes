#pragma once
#include <cstdint>
#include "../asset/hash.hpp"
#include "../core/fixed.hpp"
#include "input_types.hpp"

// Крошечная детерминированная сим для гейтов: интегрирует позицию по осям Move и считает
// прыжки/выстрелы по edge-Action. Хеш состояния (fnv1a) — golden sim-hash. Только fix32.
namespace input {

// Идентификаторы действий/осей демо-сим.
enum SimAction { A_Jump = 0, A_Fire = 1 };
enum SimAxis { AX_MoveX = 0, AX_MoveY = 1, AX_AimX = 2, AX_AimY = 3 };

struct SimState {
    fix32 x, y, aim;
    uint32_t jumps = 0, shots = 0;
};

inline void sim_step(SimState& s, const InputFrame& f) {
    fix32 step = fix32::from_float(0.25);
    s.x = s.x + f.axes[AX_MoveX] * step;
    s.y = s.y + f.axes[AX_MoveY] * step;
    s.aim = s.aim + f.axes[AX_AimX] * step;
    if (f.action_pressed(A_Jump)) ++s.jumps;
    if (f.action_pressed(A_Fire)) ++s.shots;
}

inline uint64_t hash_state(const SimState& s, const InputFrame& f, uint64_t h) {
    h = asset::fnv1a(&s.x.raw, sizeof(s.x.raw), h);
    h = asset::fnv1a(&s.y.raw, sizeof(s.y.raw), h);
    h = asset::fnv1a(&s.aim.raw, sizeof(s.aim.raw), h);
    h = asset::fnv1a(&s.jumps, sizeof(s.jumps), h);
    h = asset::fnv1a(&s.shots, sizeof(s.shots), h);
    h = asset::fnv1a(&f.held, sizeof(f.held), h);
    h = asset::fnv1a(&f.pressed, sizeof(f.pressed), h);
    h = asset::fnv1a(&f.released, sizeof(f.released), h);
    for (int a = 0; a < MAX_AXES; ++a) h = asset::fnv1a(&f.axes[a].raw, sizeof(f.axes[a].raw), h);
    return h;
}

} // namespace input
