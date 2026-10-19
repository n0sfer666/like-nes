#pragma once
#include <webgpu/webgpu.h>

#include <span>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "rumble_credits.hpp"
#include "rumble_fighter.hpp"
#include "rumble_flow.hpp"
#include "rumble_level.hpp"
#include "viewport_fit.hpp"

namespace rumble {

WGPUColor background(const Level& level, WGPUTextureFormat format);
void submit_quads(const GpuContext& gpu, WGPUTextureView view, WGPUColor clear,
                  const framework::graphics::PixelRect& shown, const render::QuadRenderer& quads,
                  std::span<const render::QuadRun> runs);
void draw_frame(const GpuContext& gpu, WGPUSurface surface, WGPUTextureView view, WGPUColor clear,
                const framework::graphics::PixelRect& shown, const render::QuadRenderer& quads,
                std::span<const render::QuadRun> runs);
bool init_quads(render::QuadRenderer& quads, const GpuContext& gpu, WGPUTextureFormat format,
                const Level& level, const Fighters& fighters, const Credits& credits);
bool init_menu_quads(render::QuadRenderer& quads, const GpuContext& gpu, WGPUTextureFormat format,
                     const Credits& credits);
// Перезаливка текстур под сцену на вершине: у меню и у уровня свои номера текстур.
bool bind_scene(render::QuadRenderer& quads, const GpuContext& gpu, WGPUTextureFormat format, const Flow& flow,
                const Credits& credits, WGPUColor& clear);

} // namespace rumble
