#include "combat.hpp"
#include "sim.hpp"
#include "world.hpp"

#include <cstdio>

// Регресс-тест детерминизма боя (S7, гейт): скриптованный combat-сценарий (движение +
// авто-огонь) → канон. sim-hash. Проверяет run-to-run идентичность; golden-хеш сверяется
// в CI (fix32/целочисл. → cross-arch). Без рендера → собирается/гоняется на всех ОС.

namespace game {

uint64_t run_scripted(int ticks, GameState* out = nullptr) {
    flecs::world w;
    GameState gs;
    spawn(w, gs);
    const fix32 dt = fix32::from_float(1.0 / 60);
    // Ввод на границе (авторинг): фикс. паттерн осей (dodge) + постоянный огонь.
    static const double AX[4] = {0.6, 0.6, -0.35, 0.2};
    static const double AY[4] = {0.5, -0.5, 0.3, -0.6};
    for (int t = 0; t < ticks; ++t) {
        input::InputFrame f;
        f.tick = static_cast<uint32_t>(t);
        f.held = 1ull << A_Fire;                       // авто-огонь (удержание)
        if (t == 0) f.pressed = 1ull << A_Fire;        // edge → Intro→Play (без рестарта позже)
        const int ph = (t / 40) % 4;
        f.axes[AX_MoveX] = fix32::from_float(AX[ph]);
        f.axes[AX_MoveY] = fix32::from_float(AY[ph]);
        step(w, gs, f, dt);
    }
    if (out) *out = gs;
    return sim_hash(w, gs);
}

} // namespace game

int main() {
    game::GameState g{};
    const uint64_t a = game::run_scripted(1200, &g);   // Intro→Play→Boss (таймаут 540) → бой
    const uint64_t b = game::run_scripted(1200);
    std::printf("[game-sim] combat-golden-hash = 0x%016llx\n", (unsigned long long)a);
    std::printf("[game-sim] final phase=%u kills=%u score=%u lives=%d\n", g.phase, g.kills, g.score, g.lives);
    std::printf("[game-sim] run1==run2: %s\n", a == b ? "YES" : "NO");
    if (a != b) { std::printf("game-sim: FAIL (nondeterministic)\n"); return 1; }
    std::printf("game-sim: PASS\n");
    return 0;
}
