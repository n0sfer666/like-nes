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
//
// Вытеснения нет, поэтому размер цели обязан быть постоянным в пределах сессии: сегодня его держит
// единственный вызывающий — Renderer ставит w_/h_ один раз и зовёт build_targets только из init, хотя
// окно demo_main изменяемое. Пересборка целей на ресайзе дала бы по слоту на каждый встреченный
// размер до shutdown — рост VRAM, неотличимый от утечки драйвера (аудит #21 A·3·13). Потолок превращает его в отказ вслух: acquire сверх MAX_SLOTS
// уникальных дескрипторов возвращает nullptr и пишет в stderr, а не растёт молча.
class TargetArena {
public:
    static constexpr size_t MAX_SLOTS = 32;

    void init(WGPUDevice device);
    void begin_frame();
    [[nodiscard]] WGPUTextureView acquire(const TargetDesc& desc);
    void shutdown();

    size_t pool_size() const { return pool_.size(); }
    uint64_t allocations() const { return allocations_; }
    uint64_t refusals() const { return refusals_; }

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
    uint64_t refusals_ = 0;
};
