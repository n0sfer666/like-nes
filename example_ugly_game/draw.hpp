#pragma once
#include <webgpu/webgpu.h>

#include "batch.hpp"
#include "fx.hpp"
#include "world.hpp"

namespace game {

void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas);
void push_hud(SpriteBatch& batch, flecs::world& world, const Atlas& atlas, const GameState& gs);
void push_screen(SpriteBatch& batch, const Atlas& atlas, const GameState& gs);
void push_toast(SpriteBatch& batch, const Atlas& atlas, const char* name, uint32_t left);
// Частицы подаются ОТСЮДА, а не из `Fx`: сам он про WebGPU не знает намеренно (см. `Fx::draw`), и
// подача кадра собрана в одном файле, а не в двух.
void push_fx(SpriteBatch& batch, Fx& fx, const Atlas& atlas);

} // namespace game
