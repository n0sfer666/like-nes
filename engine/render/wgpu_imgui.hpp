#pragma once
#include "gpu.hpp"
#include <webgpu/webgpu.h>
#include <cstdint>

// Общая WebGPU+ImGui оконная обвязка для UI-shell'ов (editor + plugin). Убирает дублирование
// boilerplate. Рендер-бэкенд ImGui = WebGPU (wgpu-native, как render #2 — НЕ deprecated OpenGL).
namespace wgpu_imgui {

// Сконфигурировать surface под окно; вернуть выбранный формат (для ImGui_ImplWGPU RenderTargetFormat).
WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d, uint32_t w, uint32_t h);

// Отрисовать текущий draw-data ImGui в ЛЮБОЙ таргет: pass + submit, без present. Отдельно от
// present, потому что цель кадра бывает не swapchain — гейт 6 снимает тот же кадр в offscreen
// текстуру, чтобы отдать PNG доказательством. Формат view обязан совпадать с тем, которым
// инициализирован ImGui_ImplWGPU (иначе валидация пайплайна).
void draw_into(const GpuContext& gpu, WGPUTextureView view, WGPUColor clear);

// Один кадр: surface-texture → render pass (clear) → ImGui draw-data → submit → present + release
// всех GPU-ресурсов кадра. false, если surface не готов (кадр пропускается). Вызывать после ImGui::Render().
bool present(const GpuContext& gpu, WGPUSurface surface, WGPUColor clear);

} // namespace wgpu_imgui
