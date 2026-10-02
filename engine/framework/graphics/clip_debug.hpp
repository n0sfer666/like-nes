#pragma once
#include <cstdint>

#include "clip_read.hpp"
#include "debug_draw.hpp"

// Кадр клипа `LNCL` на экране (спека #24, В6б): клетка листа встаёт якорем на пивот, боксы лежат от
// пивота, флип H зеркалит и то и другое вокруг пивота. Числа — целые экранные пиксели: пивот уже
// привязан к сетке, а масштаб целый, поэтому край спрайта и рамка бокса совпадают до пикселя.
namespace framework::graphics {

struct CelPlace {
    int32_t x = 0;
    int32_t y = 0;
    int32_t zoom = 1;
    bool flip_h = false;
};

struct ScreenRect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
};

ScreenRect cel_screen_rect(const ClipCel& cel, const CelPlace& at);
ScreenRect box_screen_rect(const Rect16& box, const CelPlace& at);

constexpr uint32_t CEL_DEBUG_RGBA = 0x00ffffffu;
constexpr uint32_t PIVOT_DEBUG_RGBA = 0xffffffffu;
uint32_t box_debug_rgba(BoxKind kind);

// Оверлей F3: рамка клетки, крест пивота и рамки боксов всех видов. Кадр вне клипа не рисует ничего.
void draw_cel_debug(DebugDraw& dd, const ClipView& view, uint16_t frame, const CelPlace& at);

} // namespace framework::graphics
