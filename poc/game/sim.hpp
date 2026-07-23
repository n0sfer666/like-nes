#pragma once
#include "world.hpp"
#include "input_types.hpp"

namespace game {

constexpr uint64_t SHIP_GUID = 1;
constexpr int STAR_COUNT = 96;

void spawn(flecs::world& world);
void step(flecs::world& world, const input::InputFrame& in, fix32 dt);

} // namespace game
