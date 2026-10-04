#pragma once
#include <cstdint>

#include "viewport.hpp"

// Политика вьюпорта (спека #24, В7). Игровая зона — один и тот же прямоугольник мира на любом
// экране, поэтому масштаб только целый: дробный рвал бы пиксельную сетку. Остаток экрана
// показывает мир за зоной, но не дальше предела: экран шире 21:9 открывал бы то, что уровень не
// обязан рисовать, — дальше предела заливка цветом фона карты.
namespace framework::graphics {

struct PixelSize {
    uint32_t w = 0;
    uint32_t h = 0;
};

struct PixelRect {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;
};

constexpr PixelSize VIEW_ZONE{384, 216};
constexpr PixelSize VIEW_LIMIT{576, 312};

struct ViewportFit {
    Viewport view{};
    uint32_t scale = 0;
    // Видимое в мировых единицах — то, что бейк обязан закрыть слоями (`layer_cover`).
    PixelSize visible{};
    PixelRect zone{};
    // Вне `shown` — заливка цветом фона карты, а не мир.
    PixelRect shown{};
    // Экран вне зоны: полосы HUD и тача (#26). Пустые полосы не выдаются.
    PixelRect strips[4]{};
    uint32_t strip_count = 0;
    // Окно меньше зоны: зона обрезана по центру, игрок видит не всё, что видит симуляция.
    bool cropped = false;
};

// Половина зоны в мировых единицах — `CameraConfig::half_view` под этой политикой. Кламп центра от
// окна не зависит, и бейк (`layer_cover`) считает по ней тот же отрезок центров, что и камера.
Vec2 view_zone_half(PixelSize zone = VIEW_ZONE);

// Нулевая сторона зоны — отказ: `scale == 0`, считать по такому виду нечего.
ViewportFit viewport_fit(PixelSize screen, PixelSize zone = VIEW_ZONE);

} // namespace framework::graphics
