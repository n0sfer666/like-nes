#pragma once
#include "gpu.hpp"

namespace game {

// LIKENES_GPU_BACKEND=vulkan|d3d12|d3d11|metal|gl и LIKENES_GPU_POWER=low|high в поля GpuContext.
// Вызывать до GpuContext::init: после него выбор адаптера уже сделан. Зачем нужны сами ручки —
// в комментарии к полям в gpu.hpp.
void apply_gpu_env(GpuContext& gpu);

} // namespace game
