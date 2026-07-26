#pragma once
#include <webgpu/webgpu.h>

// Мелкий хелпер: создать WGSL шейдер-модуль. Остальной boilerplate — инлайн в пассах.
WGPUShaderModule make_shader(WGPUDevice device, const char* wgsl);
