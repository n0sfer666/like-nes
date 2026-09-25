#pragma once
#include "gpu.hpp"
#include "surface_frame.hpp"
#include <webgpu/webgpu.h>
#include <cstdint>

// Общая WebGPU+ImGui оконная обвязка для UI-shell'ов (editor + plugin). Убирает дублирование
// boilerplate. Рендер-бэкенд ImGui = WebGPU (wgpu-native, как render #2 — НЕ deprecated OpenGL).
namespace wgpu_imgui {

// Отрисовать текущий draw-data ImGui в ЛЮБОЙ таргет: pass + submit, без present. Отдельно от
// present, потому что цель кадра бывает не swapchain — гейт 6 снимает тот же кадр в offscreen
// текстуру, чтобы отдать PNG доказательством. Формат view обязан совпадать с тем, которым
// инициализирован ImGui_ImplWGPU (иначе валидация пайплайна).
void draw_into(const GpuContext& gpu, WGPUTextureView view, WGPUColor clear);

// Один кадр: surface-texture → render pass (clear) → ImGui draw-data → submit → present + release
// всех GPU-ресурсов кадра. Вызывать после ImGui::Render(). `Lost` — поверхность или устройство
// потеряны насовсем, причина напечатана с префиксом `tag`, shell выходит ненулевым кодом.
enum class Presented { Drawn, Skipped, Lost };
Presented present(const GpuContext& gpu, const SurfaceSpec& spec, const char* tag, bool& warned,
                  WGPUColor clear);

} // namespace wgpu_imgui
