#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>
#include <vector>

namespace game {

std::vector<uint8_t> readback_rgba(WGPUDevice device, WGPUQueue queue, WGPUTexture tex,
                                   uint32_t w, uint32_t h);
bool write_png(const char* path, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h);

} // namespace game
