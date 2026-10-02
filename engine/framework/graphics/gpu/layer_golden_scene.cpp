#include <string>

#include "layer_golden.hpp"
#include "visual_bake.hpp"

namespace layer_golden {
namespace {

using namespace framework::tilemap;

constexpr uint64_t TEX_A = 0xA1;
constexpr uint64_t TEX_B = 0xB2;
constexpr uint64_t TEX_SKY = 0xC3;

void put(Image& im, uint32_t x, uint32_t y, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    uint8_t* p = im.rgba.data() + 4 * (std::size_t{y} * im.w + x);
    p[0] = static_cast<uint8_t>(r);
    p[1] = static_cast<uint8_t>(g);
    p[2] = static_cast<uint8_t>(b);
    p[3] = static_cast<uint8_t>(a);
}

Image blank(uint32_t w, uint32_t h) {
    Image im;
    im.w = w;
    im.h = h;
    im.rgba.assign(std::size_t{4} * w * h, 0);
    return im;
}

// Градиент по x в красном и по y в зелёном различает все восемь ориентаций, а один прозрачный
// тексель вне диагонали проверяет смешивание с небом под тайлом.
Image tiles_a() {
    Image im = blank(128, 64);
    for (uint32_t y = 0; y < im.h; ++y)
        for (uint32_t x = 0; x < im.w; ++x) {
            const uint32_t lx = x % 16, ly = y % 16, t = (y / 16) * 8 + x / 16;
            put(im, x, y, 8 + lx * 15, 8 + ly * 15, 40 + t * 7, lx == 3 && ly == 12 ? 0 : 255);
        }
    return im;
}

// Поля и зазоры — пурпур: в снимке `--out` тексель мимо раскладки margin/spacing виден глазом.
// Саму раскладку судит тест текселей: эталон и квады зовут одну `tile_texels` и разойтись не могут.
Image tiles_b() {
    Image im = blank(72, 36);
    for (uint32_t y = 0; y < im.h; ++y)
        for (uint32_t x = 0; x < im.w; ++x) put(im, x, y, 255, 0, 255, 255);
    for (uint32_t t = 0; t < 8; ++t)
        for (uint32_t ly = 0; ly < 16; ++ly)
            for (uint32_t lx = 0; lx < 16; ++lx)
                put(im, 1 + (t % 4) * 18 + lx, 1 + (t / 4) * 18 + ly, 200 - lx * 9, 30 + ly * 13,
                    100 + t * 20, 255);
    return im;
}

Image sky() {
    Image im = blank(64, 32);
    for (uint32_t y = 0; y < im.h; ++y)
        for (uint32_t x = 0; x < im.w; ++x) put(im, x, y, 20 + x * 3, 60 + y * 5, 150, 255);
    return im;
}

uint16_t cell(uint16_t index, uint16_t flips) { return static_cast<uint16_t>(index | flips); }

VisualLayerSrc tile_layer() {
    VisualLayerSrc l;
    l.name = "tiles";
    l.cells.assign(10 * 5, 0);
    const uint16_t D = CELL_FLIP_D, V = CELL_FLIP_V, Hf = CELL_FLIP_H;
    const uint16_t combos[8] = {0, Hf, V, static_cast<uint16_t>(Hf | V), D,
                                static_cast<uint16_t>(D | Hf), static_cast<uint16_t>(D | V),
                                static_cast<uint16_t>(D | Hf | V)};
    for (uint16_t i = 0; i < 8; ++i) {
        l.cells[10 + 1 + i] = cell(1, combos[i]);
        l.cells[30 + 1 + i] = cell(32, combos[i]);
    }
    l.cells[20 + 1] = cell(33, 0);
    l.cells[20 + 2] = cell(36, 0);
    l.cells[20 + 3] = cell(40, Hf);
    l.cells[20 + 4] = cell(2, 0);
    l.cells[20 + 5] = cell(2, static_cast<uint16_t>(D | V));
    l.cells[20 + 6] = cell(37, static_cast<uint16_t>(D | Hf));
    return l;
}

VisualLayerSrc sky_layer() {
    VisualLayerSrc l;
    l.name = "sky";
    l.kind = LayerKind::Image;
    l.repeat = REPEAT_X | REPEAT_Y;
    l.parallax_x = fix32::from_raw(fix32::ONE / 2);
    l.parallax_y = fix32::from_raw(fix32::ONE / 2);
    l.offset_x = fix32::from_int(5);
    l.image_guid = TEX_SKY;
    l.image_w = 64;
    l.image_h = 32;
    return l;
}

// Слой с нулевой непрозрачностью не виден вовсе: так голден судит и порядок каналов tint, который
// на белом непрозрачном цвете неотличим от любой перестановки.
VisualLayerSrc ghost_layer() {
    VisualLayerSrc l;
    l.name = "ghost";
    l.opacity = 0;
    l.cells.assign(10 * 5, 0);
    for (uint16_t x = 0; x < 10; ++x) l.cells[x] = l.cells[40 + x] = cell(1, 0);
    return l;
}

} // namespace

bool build_scene(Scene& s) {
    VisualMapSrc m;
    m.name = "golden";
    m.width = 10;
    m.height = 5;
    m.tile_size = 16;
    m.tilesets = {VisualTileset{TEX_A, 1, 32, 8, 0, 0, 0}, VisualTileset{TEX_B, 33, 8, 4, 1, 2, 0}};
    m.anims = {VisualAnimSrc{2, {{5, 3}, {6, 4}}}};
    m.layers = {sky_layer(), tile_layer(), ghost_layer()};
    std::string error;
    const std::vector<VisualMapSrc> maps{m};
    if (!bake_visuals(maps, s.bytes, error) || !s.table.open(s.bytes.data(), s.bytes.size()))
        return false;
    s.map = s.table.map(0);
    s.textures[0] = tiles_a();
    s.textures[1] = tiles_b();
    s.textures[2] = sky();
    const uint64_t guids[TEXTURES] = {TEX_A, TEX_B, TEX_SKY};
    for (uint32_t i = 0; i < TEXTURES; ++i) {
        s.guids[i] = guids[i];
        s.sizes[i] = {s.textures[i].w, s.textures[i].h};
    }
    return s.map.layers.size() == 3;
}

Frame draw_view(const Scene& s, const View& v, SpriteList& list, Batch* batches, uint32_t max) {
    LayerFrame f;
    f.view.screen_half = {fix32::from_int(W / 2), fix32::from_int(H / 2)};
    f.view.zoom = fix32::from_int(v.zoom);
    f.camera.center = {fix32::from_int(v.cx), fix32::from_int(v.cy)};
    f.tick = v.tick;
    f.textures = s.guids;
    list.clear();
    for (uint32_t i = 0; i < s.map.layers.size(); ++i)
        draw_layer(list, s.map, s.map.layers[i], f, static_cast<int16_t>(i));
    Frame out;
    out.batches = list.build(batches, max);
    out.sprites = list.count();
    out.dropped = list.dropped();
    return out;
}

} // namespace layer_golden
