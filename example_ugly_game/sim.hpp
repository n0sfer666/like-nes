#pragma once
#include "world.hpp"
#include "input_types.hpp"
#include "fx_events.hpp"

namespace game {

constexpr uint64_t SHIP_GUID = 1;
constexpr int STAR_COUNT = 96;

void spawn(flecs::world& world, GameState& gs);
void step(flecs::world& world, GameState& gs, const input::InputFrame& in, fix32 dt,
          FxSink* fx = nullptr);

} // namespace game
