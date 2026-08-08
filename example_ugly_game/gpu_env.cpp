#include "gpu_env.hpp"

#include <cstdio>
#include <string>

#include "platform_env.hpp"

namespace game {

void apply_gpu_env(GpuContext& gpu) {
    std::string v;
    if (platform::env_var("LIKENES_GPU_BACKEND", v)) {
        if (v == "vulkan" || v == "vk") gpu.backend = WGPUBackendType_Vulkan;
        else if (v == "d3d12" || v == "dx12") gpu.backend = WGPUBackendType_D3D12;
        else if (v == "d3d11" || v == "dx11") gpu.backend = WGPUBackendType_D3D11;
        else if (v == "metal") gpu.backend = WGPUBackendType_Metal;
        else if (v == "gl" || v == "opengl") gpu.backend = WGPUBackendType_OpenGL;
        // Молча игнорировать нельзя: опечатка в имени бэкенда неотличима от «эта ручка не
        // помогла», и диагностический прогон даёт ложный отрицательный ответ.
        else
            std::fprintf(stderr, "[gpu] LIKENES_GPU_BACKEND=%s not recognised - the choice is left "
                                 "to wgpu (vulkan|d3d12|d3d11|metal|gl)\n", v.c_str());
    }
    if (platform::env_var("LIKENES_GPU_POWER", v) && (v == "low" || v == "integrated"))
        gpu.power = WGPUPowerPreference_LowPower;
}

} // namespace game
