#pragma once
#include <cstdint>

namespace game {

// Фазы игрового потока (S8). Часть sim-состояния (детерм. переходы).
enum GamePhase : uint32_t { PH_Intro = 0, PH_Play = 1, PH_Boss = 2, PH_Victory = 3, PH_GameOver = 4 };

// Синглтон состояния боя (часть sim-состояния; хешируется).
struct GameState {
    uint32_t rng = 0x1234567u;    // LCG спавна врагов
    uint32_t tick = 0;
    uint32_t seq = 1;             // следующий EntId
    uint32_t fire_cd = 0;         // тиков до следующего выстрела
    uint32_t spawn_cd = 0;        // тиков до следующего врага
    uint32_t score = 0;
    int32_t lives = 3;
    uint32_t phase = PH_Intro;    // S8: игровой поток
    uint32_t phase_t = 0;         // тиков в текущей фазе
    uint32_t kills = 0;           // убито врагов (→ порог спавна босса)
};


} // namespace game
