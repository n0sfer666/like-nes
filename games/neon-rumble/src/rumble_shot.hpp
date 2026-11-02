#pragma once
#include <webgpu/webgpu.h>

#include <span>
#include <string>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "viewport_fit.hpp"

namespace rumble {

struct ShotTarget {
    WGPUTextureFormat format;
    uint32_t width;
    uint32_t height;
    WGPUColor clear;
    framework::graphics::PixelRect shown;
};

bool shoot(const GpuContext& gpu, const ShotTarget& target, const render::QuadRenderer& quads,
           std::span<const render::QuadRun> runs, const std::string& path);

struct WindowRun;
bool shoot_due(const GpuContext& gpu, const ShotTarget& target, const render::QuadRenderer& quads,
               std::span<const render::QuadRun> runs, const WindowRun& run, uint32_t tick, uint32_t frame);

} // namespace rumble
