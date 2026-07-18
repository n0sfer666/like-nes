#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include "fixed.hpp"

#include <cstdio>
#include <cstdlib>

// wgpu-native вызывает request-колбэки СИНХРОННО на нативе — забираем результат в userdata.
static WGPUAdapter request_adapter(WGPUInstance instance, WGPUSurface surface) {
    WGPUAdapter out = nullptr;
    WGPURequestAdapterOptions opts = {};
    opts.nextInChain = nullptr;
    opts.compatibleSurface = surface;
    opts.powerPreference = WGPUPowerPreference_HighPerformance;
    opts.backendType = WGPUBackendType_Undefined; // macOS → Metal
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

static WGPUDevice request_device(WGPUAdapter adapter) {
    WGPUDevice out = nullptr;
    WGPUDeviceDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.label = "like-nes-poc-device";
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

static WGPUTextureFormat configure_surface(WGPUSurface surface, WGPUAdapter adapter,
                                           WGPUDevice device, uint32_t w, uint32_t h) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface, adapter, &caps);
    WGPUTextureFormat format = caps.formatCount > 0 ? caps.formats[0]
                                                    : WGPUTextureFormat_BGRA8Unorm;

    WGPUSurfaceConfiguration cfg = {};
    cfg.nextInChain = nullptr;
    cfg.device = device;
    cfg.format = format;
    cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.viewFormatCount = 0;
    cfg.viewFormats = nullptr;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    cfg.width = w;
    cfg.height = h;
    cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &cfg);

    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return format;
}

int main() {
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(960, 540, "like-nes PoC", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "window failed\n"); glfwTerminate(); return 1; }

    WGPUInstance instance = wgpuCreateInstance(nullptr);
    if (!instance) { std::fprintf(stderr, "instance failed\n"); return 1; }

    WGPUSurface surface = glfwGetWGPUSurface(instance, window);
    WGPUAdapter adapter = request_adapter(instance, surface);
    if (!adapter) { std::fprintf(stderr, "no adapter\n"); return 1; }
    WGPUDevice device = request_device(adapter);
    if (!device) { std::fprintf(stderr, "no device\n"); return 1; }
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    WGPUTextureFormat format =
        configure_surface(surface, adapter, device, (uint32_t)fbw, (uint32_t)fbh);
    std::printf("[poc] webgpu up: fb=%dx%d format=%d\n", fbw, fbh, (int)format);

    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        WGPUSurfaceTexture st = {};
        wgpuSurfaceGetCurrentTexture(surface, &st);
        if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            int nw, nh; glfwGetFramebufferSize(window, &nw, &nh);
            if (nw > 0 && nh > 0)
                configure_surface(surface, adapter, device, (uint32_t)nw, (uint32_t)nh);
            continue;
        }

        WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);

        // Анимированный clear-color: фаза считается в fix32 (симуляционный домен),
        // в double конвертируется ТОЛЬКО здесь, на границе рендера (инвариант №2/3).
        fix32 phase = fix32::from_int(frames) * fix32::from_float(0.02);
        double t = phase.to_double();
        double pulse = 0.5 + 0.5 * (t - (double)(long)t); // пила 0..1 без float-математики в симе

        WGPURenderPassColorAttachment color = {};
        color.nextInChain = nullptr;
        color.view = view;
        color.resolveTarget = nullptr;
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        color.clearValue = WGPUColor{0.1 * pulse, 0.2, 0.35 + 0.3 * pulse, 1.0};

        WGPURenderPassDescriptor rp = {};
        rp.nextInChain = nullptr;
        rp.label = "clear-pass";
        rp.colorAttachmentCount = 1;
        rp.colorAttachments = &color;
        rp.depthStencilAttachment = nullptr;
        rp.occlusionQuerySet = nullptr;
        rp.timestampWrites = nullptr;

        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
        wgpuQueueSubmit(queue, 1, &cmd);
        wgpuSurfacePresent(surface);

        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(st.texture);

        if (++frames >= 120) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();
    std::printf("[poc] clean exit after %d rendered frames\n", frames);
    return 0;
}
