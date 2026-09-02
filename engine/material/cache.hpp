#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>
#include <vector>

#include "instance.hpp"
#include "table.hpp"

namespace mat {

// Шов «материал → пайплайн»: индекс в таблице материалов на входе, готовый пайплайн на выходе.
//
// Ключ кэша — ПАРА (точка входа, смешивание), а не индекс материала: инстанс делит с базой и то и
// другое, поэтому семь материалов библиотеки дают три пайплайна, а не семь. Ровно это утверждение
// и есть инвариант 2 спеки #18, и счётчик `pipelines_created()` выставлен наружу, чтобы его можно
// было проверить числом, а не глазом.
struct PipelineKey {
    uint64_t entry;   // fnv1a имени точки входа
    uint8_t blend;
};

struct CacheDesc {
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUTextureFormat target = WGPUTextureFormat_RGBA8Unorm;
    const Table* table = nullptr;
    const char* wgsl = nullptr;   // текст модуля библиотеки эффектов
};

class Cache {
public:
    bool init(const CacheDesc& d);
    void shutdown();

    // Пайплайн материала. NULL НЕ ВОЗВРАЩАЕТСЯ НИКОГДА: материал без точки входа или пайплайн,
    // который не собрался, отдают запасной — заметный, но живой (гейт 4 спеки #18). Чёрный экран
    // и молчание — это одно и то же событие для того, кто смотрит, а розовый квад и строка в
    // stderr называют виновника.
    WGPURenderPipeline pipeline(uint32_t material);

    // Собрать пайплайны всех материалов таблицы заранее. После этого `pipelines_created()` обязан
    // перестать расти — компиляция В КАДРЕ и есть тот фриз, ради которого написан инвариант 3.
    void warm_up();

    WGPUBindGroupLayout layout() const { return bgl_; }

    uint32_t pipelines_created() const { return created_; }
    uint32_t fallbacks() const { return fallbacks_; }

private:
    struct Entry {
        PipelineKey key;
        WGPURenderPipeline pipe;
    };

    WGPURenderPipeline build(const char* entry, uint8_t blend);
    WGPURenderPipeline find(const PipelineKey& k) const;

    WGPUDevice device_ = nullptr;
    WGPUTextureFormat target_ = WGPUTextureFormat_RGBA8Unorm;
    const Table* table_ = nullptr;
    const char* wgsl_ = nullptr;
    WGPUShaderModule module_ = nullptr;
    WGPUShaderModule fallback_module_ = nullptr;
    WGPUBindGroupLayout bgl_ = nullptr;
    WGPUPipelineLayout layout_ = nullptr;
    WGPURenderPipeline fallback_ = nullptr;
    std::vector<Entry> entries_;
    uint32_t created_ = 0;
    uint32_t fallbacks_ = 0;
};

// Раскладка вершинных буферов библиотеки: quad-VB (позиция+uv) и инстансный буфер `Instance`.
// Живёт рядом с кэшем, потому что её обязаны знать ОБА — и тот, кто строит пайплайн, и тот, кто
// подаёт буферы: разъехавшись, они дают не ошибку, а мусор на экране.
constexpr uint32_t QUAD_STRIDE = 16;
constexpr uint32_t INSTANCE_STRIDE = sizeof(Instance);

} // namespace mat
