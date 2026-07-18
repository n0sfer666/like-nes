#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <vector>

// Дескриптор транзитного таргета render-graph.
struct TargetDesc {
    uint32_t w, h;
    WGPUTextureFormat format;
    WGPUTextureUsage usage;
    bool operator==(const TargetDesc& o) const {
        return w == o.w && h == o.h && format == o.format && usage == o.usage;
    }
};

// Аренный пул транзитных render-target'ов (resource-manager графа).
// Инвариант #5 спеки #1: в submit-пути НЕТ per-frame heap/GPU-аллокаций.
// Текстуры создаются ЛЕНИВО один раз под каждый уникальный дескриптор и
// переиспользуются между кадрами; begin_frame() ничего не освобождает.
class TargetArena {
public:
    void init(WGPUDevice device);
    void begin_frame();
    WGPUTextureView acquire(const TargetDesc& desc);
    void shutdown();

    size_t pool_size() const { return pool_.size(); }
    uint64_t allocations() const { return allocations_; }

private:
    struct Slot {
        TargetDesc desc;
        WGPUTexture texture;
        WGPUTextureView view;
        bool in_use;
    };
    WGPUDevice device_ = nullptr;
    std::vector<Slot> pool_;
    uint64_t allocations_ = 0;
};
