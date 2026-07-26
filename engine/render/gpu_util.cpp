#include "gpu_util.hpp"

WGPUShaderModule make_shader(WGPUDevice device, const char* wgsl) {
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = wgsl;
    WGPUShaderModuleDescriptor desc = {};
    desc.nextInChain = &wgsl_desc.chain;
    return wgpuDeviceCreateShaderModule(device, &desc);
}
