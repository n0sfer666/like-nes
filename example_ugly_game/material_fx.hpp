#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>
#include <string>

#include "material_cache.hpp"
#include "param.hpp"
#include "material_table.hpp"

namespace game {

// Второй потребитель библиотеки эффектов (спека #18): игра-образец берёт материалы ИЗ БАНДЛА, а не
// печёт их в процессе, как golden-харнесс движка. Разница несущая: харнесс проверяет, что материал
// доезжает до пайплайна, а этот путь — что он доезжает туда через шов ассетов, вместе с текстом
// модуля. Библиотека едет отдельным `library.bundle`, рядом с `audio.bundle`: класть её в
// `game.bundle` значило бы, что перепечь материал нельзя без tint и basisu.
//
// Бандл держится ОТКРЫТЫМ на всё время жизни: `mat::Table` — вид на mmap-регион, байтами он не
// владеет, и закрытый бандл оставил бы таблицу указывать в никуда.
class MaterialFx {
public:
    bool init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target, const char* bundle_path);
    void shutdown();

    bool ready() const { return ready_; }
    WGPUBindGroupLayout layout() const { return cache_.layout(); }
    WGPUTextureView noise_view() const { return noise_view_; }

    uint32_t count() const { return table_.count(); }
    uint32_t find(const char* name) const { return table_.find(name); }
    void params(uint32_t material, float out[mat::PARAM_BLOCK_FLOATS]) const;
    int32_t slot(uint32_t material, const char* param) const {
        return table_.slot_of(material, param);
    }
    WGPURenderPipeline pipeline(uint32_t material) { return cache_.pipeline(material); }

    uint32_t pipelines_created() const { return cache_.pipelines_created(); }
    uint32_t fallbacks() const { return cache_.fallbacks(); }

    uint32_t reloads() const { return cache_.reloads(); }

private:
    // Горячая замена живёт отдельно (`ShaderWatch`, цель `material_hot`): мобильным оболочкам нужен
    // этот класс, а наблюдение за файлами — нет, и тянуть его ради одного поля значило бы тянуть
    // вотчер платформы туда, где править `.wgsl` некому. Кэш под живыми пайплайнами меняет только он.
    friend class ShaderWatch;

    // Откат частично поднявшегося `init`: кэш гасится, таблица теряет указатели в регион, который
    // вот-вот снимется вместе с локальным бандлом.
    // Причина `nullptr` — её уже напечатал вызывающий подробнее, чем может этот шов.
    bool fail(const char* why);

    struct Bundle;
    std::shared_ptr<Bundle> bundle_;
    std::string wgsl_;
    mat::Table table_;
    mat::Cache cache_;
    WGPUTexture noise_ = nullptr;
    WGPUTextureView noise_view_ = nullptr;
    bool ready_ = false;
};

} // namespace game
