#include "validate_device.hpp"

#include <webgpu/wgpu.h>

namespace mat {
namespace {

WGPUInstanceBackendFlags backend_flag(WGPUBackendType t) {
    switch (t) {
        case WGPUBackendType_Vulkan: return WGPUInstanceBackend_Vulkan;
        case WGPUBackendType_D3D12:  return WGPUInstanceBackend_DX12;
        case WGPUBackendType_Metal:  return WGPUInstanceBackend_Metal;
        default:                     return WGPUInstanceBackend_All;
    }
}

} // namespace

const char* backend_name(WGPUBackendType backend) {
    switch (backend) {
        case WGPUBackendType_Vulkan: return "Vulkan";
        case WGPUBackendType_D3D12:  return "D3D12";
        case WGPUBackendType_Metal:  return "Metal";
        default:                     return "default";
    }
}

bool ValidationDevice::open(WGPUBackendType backend, std::string& skip_reason) {
    // Бэкенд выбирается на ИНСТАНСЕ: `WGPURequestAdapterOptions.backendType` wgpu-native
    // игнорирует, печатая об этом в лог, — просьба выглядела бы исполненной и молча не исполнялась.
    WGPUInstanceExtras extras = {};
    extras.chain.sType = static_cast<WGPUSType>(WGPUSType_InstanceExtras);
    extras.backends = backend_flag(backend);
    WGPUInstanceDescriptor idesc = {};
    idesc.nextInChain = &extras.chain;
    instance = wgpuCreateInstance(&idesc);
    if (!instance) {
        skip_reason = "instance for this backend was refused";
        return false;
    }
    WGPURequestAdapterOptions opts = {};
    opts.powerPreference = WGPUPowerPreference_HighPerformance;
    wgpuInstanceRequestAdapter(
        instance, &opts,
        [](WGPURequestAdapterStatus, WGPUAdapter a, char const*, void* ud) {
            *static_cast<WGPUAdapter*>(ud) = a;
        },
        &adapter);
    if (!adapter) {
        skip_reason = "no adapter on this machine";
        close();
        return false;
    }
    WGPUDeviceDescriptor ddesc = {};
    ddesc.label = "like-nes-material-validator";
    wgpuAdapterRequestDevice(
        adapter, &ddesc,
        [](WGPURequestDeviceStatus, WGPUDevice d, char const*, void* ud) {
            *static_cast<WGPUDevice*>(ud) = d;
        },
        &device);
    if (!device) {
        skip_reason = "adapter gave no device";
        close();
        return false;
    }
    return true;
}

void ValidationDevice::close() {
    if (device) wgpuDeviceRelease(device);
    if (adapter) wgpuAdapterRelease(adapter);
    if (instance) wgpuInstanceRelease(instance);
    device = nullptr;
    adapter = nullptr;
    instance = nullptr;
}

} // namespace mat
