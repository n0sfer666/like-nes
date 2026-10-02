#pragma once
#include <cstdio>
#include <string>
#include <vector>

#include "layer_draw.hpp"
#include "visual_bake.hpp"

// Общая фикстура тестов слоя: карта 16-пиксельных тайлов на двух тайлсетах (второй — с margin и
// spacing) и анимированный индекс 2. Экран 128x64, масштаб 1.
namespace layer_fixture {

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::graphics;
using namespace framework::tilemap;

constexpr uint32_t CAP = 4096;
inline Sprite storage[CAP];
inline uint64_t keys[CAP];
inline Batch batches[CAP];

constexpr uint64_t TEX_A = 0xA1;
constexpr uint64_t TEX_B = 0xB2;
constexpr uint64_t TEX_SKY = 0xC3;
constexpr uint64_t TEXTURES[3] = {TEX_A, TEX_B, TEX_SKY};

inline fix32 fx(int32_t v) { return fix32::from_int(v); }
inline fix32 half_of(int32_t v) { return fix32::from_raw(v * (fix32::ONE / 2)); }

inline VisualLayerSrc tile_layer(uint32_t w, uint32_t h) {
    VisualLayerSrc l;
    l.name = "tiles";
    l.cells.assign(std::size_t{w} * h, 1);
    return l;
}

struct Fixture {
    std::vector<uint8_t> bytes;
    VisualTable table;
    VisualMap map;
};

inline void bake(Fixture& f, uint32_t w, uint32_t h, std::vector<VisualLayerSrc> layers) {
    VisualMapSrc m;
    m.name = "m";
    m.width = w;
    m.height = h;
    m.tile_size = 16;
    m.tilesets = {VisualTileset{TEX_A, 1, 8, 4, 0, 0, 0}, VisualTileset{TEX_B, 9, 4, 2, 1, 2, 0}};
    m.anims = {VisualAnimSrc{2, {{5, 3}, {6, 4}}}};
    m.layers = std::move(layers);
    std::string error;
    const std::vector<VisualMapSrc> maps{m};
    check(bake_visuals(maps, f.bytes, error), "fixture bakes");
    check(f.table.open(f.bytes.data(), f.bytes.size()), "fixture opens");
    f.map = f.table.map(0);
}

inline LayerFrame frame_at(int32_t cx, int32_t cy) {
    LayerFrame f;
    f.view.screen_half = {fx(64), fx(32)};
    f.camera.center = {fx(cx), fx(cy)};
    f.textures = TEXTURES;
    return f;
}

inline LayerDrawStats run(SpriteList& list, const Fixture& fx_, const LayerFrame& f, uint32_t layer = 0) {
    list.clear();
    return draw_layer(list, fx_.map, fx_.map.layers[layer], f, 3);
}

} // namespace layer_fixture
