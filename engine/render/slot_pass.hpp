#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../material/table.hpp"
#include "material_scene.hpp"
#include "slot_textures.hpp"

// Проход ТЕКСТУРНОГО СЛОТА графа кадра (шаги B и C гейта 7, спека #18): те же квады, что рисует
// проход материалов, но в отдельную текстуру и не цветом, а величиной, которую слот несёт, —
// нормалью поверхности или долей перекрытого света. Карта берётся у слота материала: данные
// решают, чем светится и что затеняет спрайт, а не ветка в коде.
//
// Проход отдельный, а не второй таргет материального: MRT потребовал бы, чтобы КАЖДАЯ точка входа
// библиотеки возвращала структуру из нескольких locations, то есть правки закрытых гейтов 4/5/6 и
// их голдена ради шага, который к ним отношения не имеет. Цена честная и названа: сцена рисуется
// ещё раз на буфер, и `report_cost` меряет именно её.
//
// Класс ОДИН на оба буфера: геометрия, инстансы и группировка по материалу у них общие, и второй
// копией они разъехались бы ровно в том, что гейт и проверяет. Различие целиком уехало в `Desc`.
namespace slotgfx {

// Чем один буфер отличается от другого — ЦЕЛИКОМ. Имя слота адресует строку таблицы, точка входа
// выбирает фрагментную функцию модуля, цвет очистки и подстановка — значение «здесь ничего нет»,
// и эти два обязаны совпадать: пиксель, которого не накрыл спрайт, обязан быть неотличим от
// пикселя материала без слота.
struct Desc {
    const char* slot = nullptr;
    const char* entry = nullptr;
    WGPUColor clear = {};
    WGPUTextureView fallback = nullptr;
};

// Два описания, которые есть у графа кадра. Функциями, а не константами: подстановка приходит из
// банка, то есть известна только после его старта.
Desc normals_of(const slotgold::Bank& bank);
Desc occluders_of(const slotgold::Bank& bank);

class Pass {
public:
    bool init(WGPUDevice device, WGPUQueue queue, const mat::Table& table,
              const slotgold::Bank& bank, const Desc& desc, uint32_t w, uint32_t h,
              WGPUTextureView albedo, WGPUTextureFormat fmt);
    void shutdown();

    // Инстансы берутся у сцены и переупорядочиваются по материалу: карта слота привязана к
    // группе, а группа — к материалу.
    void build(const matgold::Scene& scene);

    void run(WGPUCommandEncoder enc, WGPUTextureView dst);

    // Сколько материалов получили карту ИЗ ТАБЛИЦЫ и сколько остались на подстановке. Две
    // величины, а не одна: «слота нет» и «слот есть, но ассет неизвестен» — разные события, и
    // вторая обязана быть видна, иначе опечатка в `library.mat` читается как замысел.
    uint32_t mapped() const { return mapped_; }
    uint32_t flat() const { return flat_; }
    uint32_t missing() const { return missing_; }

    // Имя ассета, выбранного для материала, или nullptr, если слота нет. Наружу — ради гейта:
    // утверждение «слот пришёл из таблицы» иначе нечем отличить от «привязали одно и то же всем».
    const char* asset(uint32_t material) const;

private:
    struct Group {
        WGPUBindGroup bg = nullptr;
        const char* asset = nullptr;
        uint32_t first = 0;
        uint32_t count = 0;
    };

    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;
    WGPUBindGroupLayout bgl_ = nullptr;
    WGPURenderPipeline pipe_ = nullptr;
    WGPUBuffer quad_vbo_ = nullptr;
    WGPUBuffer quad_ibo_ = nullptr;
    WGPUBuffer inst_vbo_ = nullptr;
    WGPUBuffer vp_ubo_ = nullptr;
    WGPUSampler sampler_ = nullptr;
    WGPUColor clear_ = {};
    std::vector<Group> groups_;
    // Ёмкость инстансного буфера в БАЙТАХ. Держится полем, потому что число инстансов — свойство
    // сцены, а буфер создаётся до неё: константа здесь означала бы, что сцена шире выписанной
    // ёмкости пишет за границу, а `build()` возвращается молча.
    std::size_t inst_bytes_ = 0;
    uint32_t instances_ = 0;
    uint32_t mapped_ = 0;
    uint32_t flat_ = 0;
    uint32_t missing_ = 0;
};

} // namespace slotgfx
