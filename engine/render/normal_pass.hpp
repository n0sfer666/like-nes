#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>
#include <vector>

#include "../material/table.hpp"
#include "material_scene.hpp"
#include "normal_textures.hpp"

// Проход нормалей графа кадра (шаг B гейта 7, спека #18): те же квады, что рисует проход
// материалов, но в отдельную текстуру и не цветом, а нормалью поверхности. Карта берётся у
// ТЕКСТУРНОГО СЛОТА материала — данные решают, чем светится спрайт, а не ветка в коде.
//
// Проход отдельный, а не второй таргет материального: MRT потребовал бы, чтобы КАЖДАЯ точка входа
// библиотеки возвращала структуру из двух locations, то есть правки закрытых гейтов 4/5/6 и их
// голдена ради шага, который к ним отношения не имеет. Цена честная и названа: сцена рисуется
// дважды, и `report_cost` меряет именно её.
namespace normgfx {

// Цвет очистки — та же плоская нормаль (0,0,1), что лежит в 1x1 текстуре банка: пиксель, которого
// спрайт не накрыл, обязан быть неотличим от пикселя, накрытого материалом без слота нормали.
constexpr double FLAT_CHANNEL = 128.0 / 255.0;
constexpr WGPUColor CLEAR = {FLAT_CHANNEL, FLAT_CHANNEL, 1.0, 1.0};

class Pass {
public:
    bool init(WGPUDevice device, WGPUQueue queue, const mat::Table& table,
              const normgold::Bank& bank, uint32_t w, uint32_t h, WGPUTextureView albedo,
              WGPUTextureFormat fmt);
    void shutdown();

    // Инстансы берутся у сцены и переупорядочиваются по материалу: карта нормали привязана к
    // группе, а группа — к материалу.
    void build(const matgold::Scene& scene);

    void run(WGPUCommandEncoder enc, WGPUTextureView dst);

    // Сколько материалов получили карту ИЗ ТАБЛИЦЫ и сколько остались плоскими. Две величины, а не
    // одна: «слота нет» и «слот есть, но ассет неизвестен» — разные события, и вторая обязана быть
    // видна, иначе опечатка в `library.mat` читается как замысел.
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
    std::vector<Group> groups_;
    uint32_t instances_ = 0;
    uint32_t mapped_ = 0;
    uint32_t flat_ = 0;
    uint32_t missing_ = 0;
};

} // namespace normgfx
