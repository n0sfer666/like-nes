#pragma once
#include <cstdint>
#include <span>

#include "graphics_sprite.hpp"
#include "quad_batch.hpp"
#include "visual_read.hpp"

// Шов кадра слоёв к GPU (спека #24, В5б): отсортированный список спрайтов → квады бэкенда.
// Регион 0 — вся текстура (image-слой), иначе тайл `LNVL` по раскладке его тайлсета.
namespace framework::graphics {

struct TextureSize {
    uint32_t w = 0;
    uint32_t h = 0;
};

struct QuadStats {
    uint32_t quads = 0;
    uint32_t runs = 0;
    // Квад, чей источник не лёг в текстуру, остаётся нулевым: бэкенд не читает за её краем.
    uint32_t rejected = 0;
    // Не хватило места в `quads` или `runs` — хвост кадра не нарисован.
    uint32_t dropped = 0;
};

QuadStats layer_quads(const SpriteList& list, std::span<const Batch> batches,
                      const tilemap::VisualMap& map, std::span<const TextureSize> sizes,
                      std::span<render::Quad> quads, std::span<render::QuadRun> runs);

} // namespace framework::graphics
