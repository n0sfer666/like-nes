#pragma once
#include <webgpu/webgpu.h>

#include "batch.hpp"
#include "world.hpp"

namespace game {

void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas);
WGPURenderPassEncoder begin_clear(WGPUCommandEncoder enc, WGPUTextureView view);

} // namespace game
