#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <string>
#include <vector>

#include "cache.hpp"
#include "hot_reload.hpp"
#include "material_scene.hpp"
#include "table.hpp"

struct GpuContext;

namespace ide::editor {

// Половина «в редакторе» гейта 3 спеки #18: библиотека эффектов, нарисованная ТЕМ ЖЕ кадром, что
// сверяет голден (`matgold::render_frame`), и обновляющаяся по правке `.wgsl`. Вторая реализация
// превью разъехалась бы с голденом ровно в том, что голден и проверяет.
//
// Картинка перерисовывается ТОЛЬКО после успешной замены, а не каждый кадр: кадр редактора не
// обязан платить readback за неизменившийся шейдер, а на битой правке прежняя картинка обязана
// остаться на экране — она и есть «предыдущий вариант жив».
class MaterialPanel {
public:
    bool init(GpuContext& gpu, const std::string& library_dir);
    void shutdown();
    void poll();
    void draw();

    const char* status() const { return status_.c_str(); }
    const char* diag() const { return diag_.c_str(); }
    uint32_t reloads() const { return cache_.reloads(); }
    uint32_t rejects() const { return hot_.rejects(); }

    // Отпечаток ПОСЛЕДНЕЙ отрисованной картинки. Наружу — ради гейта 3: «предыдущий вариант жив»
    // проверяется пикселями, а счётчик перерисовок доказал бы только то, что код не звался.
    uint64_t preview_hash() const { return preview_hash_; }

private:
    bool fail();
    bool render_preview();

    GpuContext* gpu_ = nullptr;
    std::vector<uint8_t> table_bytes_;   // таблица живёт указателями в него: `Table` его не копирует
    mat::Table table_;
    mat::Cache cache_;
    mat::HotReload hot_;
    matgold::Scene scene_;
    std::string wgsl_;
    std::string path_;
    std::string status_;
    std::string diag_;
    WGPUTexture tex_ = nullptr;
    WGPUTextureView view_ = nullptr;
    uint64_t preview_hash_ = 0;
    uint32_t draws_ = 0;
    bool ready_ = false;
};

} // namespace ide::editor
