#pragma once
#include "gpu.hpp"
#include <webgpu/webgpu.h>
#include <cstdint>

// Общая WebGPU+ImGui оконная обвязка для UI-shell'ов (editor + plugin). Убирает дублирование
// boilerplate. Рендер-бэкенд ImGui = WebGPU (wgpu-native, как render #2 — НЕ deprecated OpenGL).
namespace wgpu_imgui {

// Сконфигурировать surface под окно; вернуть выбранный формат (для ImGui_ImplWGPU RenderTargetFormat).
WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d, uint32_t w, uint32_t h);

// Один кадр: surface-texture → render pass (clear) → ImGui draw-data → submit → present + release
// всех GPU-ресурсов кадра. false, если surface не готов (кадр пропускается). Вызывать после ImGui::Render().
bool present(const GpuContext& gpu, WGPUSurface surface, WGPUColor clear);

} // namespace wgpu_imgui
