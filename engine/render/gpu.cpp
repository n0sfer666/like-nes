#include "gpu.hpp"

#include <cstdio>

namespace {

WGPUAdapter request_adapter(WGPUInstance instance, WGPUSurface surface) {
    WGPUAdapter out = nullptr;
    WGPURequestAdapterOptions opts = {};
    opts.compatibleSurface = surface;
    opts.powerPreference = WGPUPowerPreference_HighPerformance;
    opts.backendType = WGPUBackendType_Undefined;
    opts.forceFallbackAdapter = false;
    wgpuInstanceRequestAdapter(
        instance, &opts,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* msg, void* ud) {
            if (status != WGPURequestAdapterStatus_Success)
                std::fprintf(stderr, "adapter request failed: %s\n", msg ? msg : "?");
            *static_cast<WGPUAdapter*>(ud) = adapter;
        },
        &out);
    return out;
}

WGPUDevice request_device(WGPUAdapter adapter) {
    WGPUDevice out = nullptr;
    WGPUDeviceDescriptor desc = {};
    desc.label = "like-nes-render-device";
    // BC-текстуры нужны ассет-шву (baked KTX2→BC7). Best-effort: включаем, если адаптер
    // поддерживает (desktop Metal/Vulkan — да); render-путь фичу игнорирует.
    WGPUFeatureName feats[1];
    uint32_t feat_count = 0;
    if (wgpuAdapterHasFeature(adapter, WGPUFeatureName_TextureCompressionBC))
        feats[feat_count++] = WGPUFeatureName_TextureCompressionBC;
    desc.requiredFeatureCount = feat_count;
    desc.requiredFeatures = feats;
    wgpuAdapterRequestDevice(
        adapter, &desc,
        [](WGPURequestDeviceStatus status, WGPUDevice device, char const* msg, void* ud) {
            if (status != WGPURequestDeviceStatus_Success)
                std::fprintf(stderr, "device request failed: %s\n", msg ? msg : "?");
            *static_cast<WGPUDevice*>(ud) = device;
        },
        &out);
    return out;
}

const char* backend_name(WGPUBackendType t) {
    switch (t) {
    case WGPUBackendType_D3D11:    return "D3D11";
    case WGPUBackendType_D3D12:    return "D3D12";
    case WGPUBackendType_Metal:    return "Metal";
    case WGPUBackendType_Vulkan:   return "Vulkan";
    case WGPUBackendType_OpenGL:   return "OpenGL";
    case WGPUBackendType_OpenGLES: return "OpenGLES";
    case WGPUBackendType_WebGPU:   return "WebGPU";
    case WGPUBackendType_Null:     return "Null";
    default:                       return "?";
    }
}

} // namespace

bool GpuContext::init(WGPUSurface surface) {
    // instance может быть уже создан вызывающим (оконный путь: surface нужен до adapter).
    if (!instance) instance = wgpuCreateInstance(nullptr);
    if (!instance) { std::fprintf(stderr, "instance failed\n"); return false; }
    adapter = request_adapter(instance, surface);
    if (!adapter) { std::fprintf(stderr, "no adapter\n"); shutdown(); return false; }
    supports_bc = wgpuAdapterHasFeature(adapter, WGPUFeatureName_TextureCompressionBC);
    device = request_device(adapter);
    if (!device) { std::fprintf(stderr, "no device\n"); shutdown(); return false; }
    // Без этого коллбэка валидационные ошибки WebGPU не выводятся НИКУДА: пайплайн не создался,
    // отрисовки нет, окно чёрное, а причина не названа. Именно так «чёрный экран со звуком»
    // выглядел на Windows, пока шов молчал.
    wgpuDeviceSetUncapturedErrorCallback(
        device,
        [](WGPUErrorType type, char const* msg, void*) {
            std::fprintf(stderr, "[wgpu] error %u: %s\n", (unsigned)type, msg ? msg : "?");
        },
        nullptr);
    WGPUAdapterProperties props = {};
    wgpuAdapterGetProperties(adapter, &props);
    std::printf("[gpu] %s | %s | BC: %s\n", props.name ? props.name : "?",
                backend_name(props.backendType), supports_bc ? "yes" : "no");
    queue = wgpuDeviceGetQueue(device);
    if (!queue) { shutdown(); return false; }
    return true;
}

void GpuContext::shutdown() {
    if (queue) wgpuQueueRelease(queue);
    if (device) wgpuDeviceRelease(device);
    if (adapter) wgpuAdapterRelease(adapter);
    if (instance) wgpuInstanceRelease(instance);
    queue = nullptr; device = nullptr; adapter = nullptr; instance = nullptr;
}
