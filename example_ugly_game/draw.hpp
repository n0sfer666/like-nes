#pragma once
#include <webgpu/webgpu.h>

#include "batch.hpp"
#include "world.hpp"

namespace game {

void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas);
void push_hud(SpriteBatch& batch, flecs::world& world, const Atlas& atlas, const GameState& gs);
void push_screen(SpriteBatch& batch, const Atlas& atlas, const GameState& gs);
void push_toast(SpriteBatch& batch, const Atlas& atlas, const char* name, uint32_t left);
WGPURenderPassEncoder begin_clear(WGPUCommandEncoder enc, WGPUTextureView view);

} // namespace game
