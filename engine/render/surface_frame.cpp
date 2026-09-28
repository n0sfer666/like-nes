#include "surface_frame.hpp"

#include <cstdio>

FrameAction frame_action(WGPUSurfaceGetCurrentTextureStatus status, bool device_lost) {
    if (device_lost) return FrameAction::Quit;
    switch (status) {
    case WGPUSurfaceGetCurrentTextureStatus_Success:  return FrameAction::Draw;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:  return FrameAction::Skip;
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost:     return FrameAction::Reconfigure;
    default:                                          return FrameAction::Quit;
    }
}

WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d,
                                    uint32_t w, uint32_t h) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(s, a, &caps);
    const WGPUTextureFormat fmt =
        caps.formatCount ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    reconfigure_surface(SurfaceSpec{s, fmt, w, h}, d);
    return fmt;
}

void reconfigure_surface(const SurfaceSpec& spec, WGPUDevice d) {
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = d; cfg.format = spec.format; cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto; cfg.width = spec.width; cfg.height = spec.height;
    cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(spec.surface, &cfg);
}

SurfaceFrame acquire_frame(const SurfaceSpec& spec, const GpuContext& gpu, const char* tag,
                           bool& warned) {
    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(spec.surface, &st);
    const bool lost = gpu.device_lost.load();
    const FrameAction act = frame_action(st.status, lost);
    if (act == FrameAction::Draw) return SurfaceFrame{st.texture, false};
    if (st.texture) wgpuTextureRelease(st.texture);
    if (act == FrameAction::Quit) {
        std::fprintf(stderr, "[%s] surface texture status %u%s - quitting\n", tag,
                     static_cast<unsigned>(st.status), lost ? ", device lost" : "");
        return SurfaceFrame{nullptr, true};
    }
    if (act == FrameAction::Reconfigure) reconfigure_surface(spec, gpu.device);
    if (!warned) {
        std::fprintf(stderr, "[%s] surface texture status %u - frame skipped\n", tag,
                     static_cast<unsigned>(st.status));
        warned = true;
    }
    return SurfaceFrame{nullptr, false};
}
