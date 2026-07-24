#pragma once
#include "fixed.hpp"
#include "action_map.hpp"
#include <flecs.h>
#include <cstdint>

namespace game {

constexpr int VIEW_W = 960;
constexpr int VIEW_H = 540;
constexpr int HALF_W = VIEW_W / 2;
constexpr int HALF_H = VIEW_H / 2;

enum Action { A_Fire = 0 };
enum Axis { AX_MoveX = 0, AX_MoveY = 1 };

struct Transform { fix32 x, y; };
struct Velocity { fix32 x, y; };
struct Ship {};
struct Star { fix32 speed; fix32 size; uint8_t shade; };

// Боевой слой (S7/S8). Всё — целочисл./fix32 → sim-hash воспроизводим.
struct Bullet {};                 // снаряд игрока (бьёт врагов/босса)
struct Hostile {};                // снаряд босса (бьёт игрока)
struct Enemy { int32_t hp; };
struct Boss { int32_t hp; uint32_t fire_cd; int32_t dir; };  // босс: HP + кулдаун огня + верт. направление
constexpr int32_t BOSS_HP_MAX = 40;   // общий: boss_spawn + HP-бар (draw)
struct EntId { uint32_t seq; };   // монотонный id спавна → канон. порядок для хеша/коллизий

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

input::ActionMap make_map();

} // namespace game
