#pragma once
#include <cstdint>
#include <vector>

#include "clip_bake.hpp"
#include "clip_debug.hpp"
#include "clip_read.hpp"

// Пиксельный голден кадра клипа (спека #24, В6б): клетка листа и оверлей F3 через `QuadRenderer`
// против CPU-эталона, который берёт кадр и боксы из ИСХОДНИКА клипа, а не из запечённой таблицы,
// и кладёт клетку потекселью, а не прямоугольником, а цвета видов боксов держит своей таблицей, —
// общего у двух путей только лист.
namespace clip_golden {

using namespace framework::graphics;

constexpr uint32_t W = 160;
constexpr uint32_t H = 96;
constexpr uint8_t CLEAR[4] = {10, 20, 30, 255};

struct Image {
    std::vector<uint8_t> rgba;
    uint32_t w = 0;
    uint32_t h = 0;
};

struct Scene {
    ClipSrc src;
    std::vector<uint8_t> bytes;
    ClipTable table;
    ClipView clip;
    Image sheet;
    Image solid;
};

struct View {
    const char* name;
    uint64_t tick;
    CelPlace at;
};

constexpr View VIEWS[3] = {
    {"base", 2, {40, 60, 1, false}}, {"edge", 3, {40, 60, 1, false}}, {"flip", 14, {100, 84, 2, true}}};

enum class Blind { None, Flip, Boxes };

bool build_scene(Scene& s);
std::vector<uint8_t> reference(const Scene& s, const View& v, Blind blind);

} // namespace clip_golden
