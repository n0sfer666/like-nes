#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <span>
#include <vector>

#include "gpu.hpp"
#include "quad_batch.hpp"

// Общее у пиксельных голденов квадов (спека #24, В5б и В6б): кадр в offscreen-цель w×h с
// заливкой `clear` и обратное чтение RGBA8; на sRGB-цели заливка переводится в линейный свет,
// чтобы байты цели совпали с байтами unorm-цели.
namespace golden_target {

std::vector<uint8_t> render(GpuContext& gpu, const render::QuadRenderer& qr,
                            std::span<const render::QuadRun> runs, uint32_t w, uint32_t h,
                            const uint8_t clear[4],
                            WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm);

uint32_t mismatched(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);

} // namespace golden_target
