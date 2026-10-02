#pragma once
#include <cstdint>
#include <vector>

#include "layer_draw.hpp"
#include "layer_quads.hpp"
#include "visual_read.hpp"

// Пиксельный голден слоёв (спека #24, В5б): сцена из процедурных текстур, эталон — растеризация
// на CPU от тех же спрайтов, но через `tile_texels` и `flip_source`, а не через квады бэкенда.
namespace layer_golden {

using namespace framework::graphics;

constexpr uint32_t W = 160;
constexpr uint32_t H = 96;
constexpr uint8_t CLEAR[4] = {10, 20, 30, 255};
constexpr uint32_t TEXTURES = 3;

struct Image {
    std::vector<uint8_t> rgba;
    uint32_t w = 0;
    uint32_t h = 0;
};

struct Scene {
    std::vector<uint8_t> bytes;
    framework::tilemap::VisualTable table;
    framework::tilemap::VisualMap map;
    Image textures[TEXTURES];
    uint64_t guids[TEXTURES] = {};
    TextureSize sizes[TEXTURES] = {};
};

struct View {
    const char* name;
    uint64_t tick;
    int32_t cx;
    int32_t cy;
    int32_t zoom;
};

constexpr View VIEWS[3] = {{"base", 2, 80, 40, 1}, {"anim", 3, 80, 40, 1}, {"zoom", 2, 52, 30, 2}};

struct Frame {
    uint32_t sprites = 0;
    uint32_t batches = 0;
    uint32_t dropped = 0;
};

bool build_scene(Scene& s);
Frame draw_view(const Scene& s, const View& v, SpriteList& list, Batch* batches, uint32_t max);
// `flips = false` — заведомо сломанный эталон, контроль того, что сцена флипы вообще видит.
std::vector<uint8_t> reference(const Scene& s, const SpriteList& list, bool flips);

} // namespace layer_golden
