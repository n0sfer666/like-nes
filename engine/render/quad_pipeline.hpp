#pragma once
#include <webgpu/webgpu.h>

namespace render {

WGPUBindGroupLayout make_quad_layout(WGPUDevice device);
WGPURenderPipeline make_quad_pipeline(WGPUDevice device, WGPUTextureFormat target,
                                      WGPUBindGroupLayout layout);
// Формат текстур квадов под цель кадра. Тексели PNG уже в sRGB: на sRGB-цели (`formats[0]`
// Vulkan и DX12) текстура тоже sRGB — декод при чтении и кодирование при записи гасят друг друга, и
// тайл выходит тем же байтом, что в Tiled, а не светлее.
WGPUTextureFormat quad_texel_format(WGPUTextureFormat target);

} // namespace render
