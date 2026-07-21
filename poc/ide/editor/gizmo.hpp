#pragma once
#include <cmath>

// 2D-камера редактора + гизмо перемещения (спека #7, гейт 6: гизмо = отдельный immediate
// debug-draw слой). screen-space хэндлы для 2D-движка (open вопрос спеки закрыт: screen-space).
// Детерм. world↔screen round-trip (identity) — тестируется headless.
namespace ide::editor {

struct Camera2D {
    float px = 0, py = 0;   // центр камеры в world
    float zoom = 1.0f;      // пикселей на world-единицу
    float vw = 1280, vh = 720;
};

inline void world_to_screen(const Camera2D& c, float wx, float wy, float& sx, float& sy) {
    sx = (wx - c.px) * c.zoom + c.vw * 0.5f;
    sy = (wy - c.py) * c.zoom + c.vh * 0.5f;
}

inline void screen_to_world(const Camera2D& c, float sx, float sy, float& wx, float& wy) {
    float z = (c.zoom != 0.0f) ? c.zoom : 1.0f;   // guard div-by-zero
    wx = (sx - c.vw * 0.5f) / z + c.px;
    wy = (sy - c.vh * 0.5f) / z + c.py;
}

enum class GizmoPart { None, AxisX, AxisY, Center };

// Гизмо перемещения в screen-space: центр + оси X (вправо) / Y (вверх). Хит-тест точки экрана.
struct Gizmo {
    float sx = 0, sy = 0;   // экранная позиция origin (выбранной сущности)
    float len = 60.0f;      // длина оси в пикселях
    float grab = 6.0f;      // радиус захвата
};

inline GizmoPart gizmo_hit(const Gizmo& g, float px, float py) {
    float dcx = px - g.sx, dcy = py - g.sy;
    if (dcx * dcx + dcy * dcy <= g.grab * g.grab) return GizmoPart::Center;
    if (px >= g.sx && px <= g.sx + g.len && std::fabs(dcy) <= g.grab) return GizmoPart::AxisX;
    if (py <= g.sy && py >= g.sy - g.len && std::fabs(dcx) <= g.grab) return GizmoPart::AxisY;
    return GizmoPart::None;
}

} // namespace ide::editor
