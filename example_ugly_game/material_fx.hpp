#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>
#include <string>

// Путями ОТ ЭТОГО ФАЙЛА, а не через каталог цели: `cache.hpp` есть и у физики фреймворка, её
// каталог у платформера стоит в -I раньше, и короткое имя привело бы туда — с диагностикой «нет
// типа Cache в пространстве mat», ничего не говорящей про то, чей заголовок нашёлся.
#include "../engine/material/cache.hpp"
#include "../engine/material/hot_reload.hpp"
#include "../engine/material/param.hpp"
#include "../engine/material/table.hpp"

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

    // Дев-режим гейта 3 спеки #18. Путь к ИСХОДНИКУ модуля приходит снаружи и в бандле его нет:
    // бандл — байты, править в нём нечего, а игра-образец обязана уметь то же, что редактор.
    // Наблюдение не включено по умолчанию: игрок правок шейдера не делает, а вотч на каталог —
    // дескриптор и опрос каждый кадр.
    bool watch_shader(const std::string& wgsl_path);
    // Зовётся из кадра БЕЗ ожидания: кадр здесь стоит 16 мс, и любое окно ожидания вычитается
    // прямо из него. Отказ битой правки не гасит сцену — рисует прежний вариант.
    void poll_shader();
    bool watching() const { return hot_.watching(); }
    const char* watch_backend() const {
        return hot_.backend() == platform::WatchBackend::Native ? "native" : "poll";
    }
    const char* watch_error() const { return hot_.error().c_str(); }
    uint32_t reloads() const { return cache_.reloads(); }
    uint32_t rejects() const { return hot_.rejects(); }

private:
    // Откат частично поднявшегося `init`: кэш гасится, таблица теряет указатели в регион, который
    // вот-вот снимется вместе с локальным бандлом.
    // Причина `nullptr` — её уже напечатал вызывающий подробнее, чем может этот шов.
    bool fail(const char* why);

    struct Bundle;
    std::shared_ptr<Bundle> bundle_;
    std::string wgsl_;
    mat::Table table_;
    mat::Cache cache_;
    mat::HotReload hot_;
    WGPUTexture noise_ = nullptr;
    WGPUTextureView noise_view_ = nullptr;
    bool ready_ = false;
};

} // namespace game
