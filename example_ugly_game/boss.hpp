#pragma once
#include "world.hpp"

namespace game {

// Босс (S8): большой враг с HP + ответным огнём. Детерм. движение (верт. осцилляция) +
// периодический выстрел hostile-снаряда к игроку. Всё в fix32/целочисл. домене.
void boss_spawn(flecs::world& world, GameState& gs);
void boss_step(flecs::world& world, GameState& gs, fix32 dt, fix32 player_y);

} // namespace game
