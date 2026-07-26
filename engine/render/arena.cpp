#include "arena.hpp"

void TargetArena::init(WGPUDevice device) {
    device_ = device;
}

void TargetArena::begin_frame() {
    // Освобождаем логически (переиспользование), НЕ физически: без GPU-dealloc.
    for (Slot& s : pool_) s.in_use = false;
}

WGPUTextureView TargetArena::acquire(const TargetDesc& desc) {
    for (Slot& s : pool_) {
        if (!s.in_use && s.desc == desc) {
            s.in_use = true;
            return s.view;
        }
    }

    // Промах пула: ленивое создание один раз под новый дескриптор (только в warm-up).
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{desc.w, desc.h, 1};
    td.format = desc.format;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = desc.usage;
    WGPUTexture tex = wgpuDeviceCreateTexture(device_, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);
    ++allocations_;

    pool_.push_back(Slot{desc, tex, view, true});
    return view;
}

void TargetArena::shutdown() {
    for (Slot& s : pool_) {
        if (s.view) wgpuTextureViewRelease(s.view);
        if (s.texture) wgpuTextureRelease(s.texture);
    }
    pool_.clear();
    device_ = nullptr;
}
