#pragma once
#include "fixed.hpp"
#include "game_state.hpp"
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


} // namespace game
