#include "sprite_out.hpp"

#include <cmath>

namespace game {
namespace {

// Порядок байт в `Sprite::rgba` — `0xRRGGBBAA`. Фреймворк его НЕ трактует (`lerp_rgba` идёт по
// каналам симметрично), поэтому написать его обязан потребитель, и написан он ровно здесь, в
// единственном месте, где цвет спрайта становится цветом пикселя.
float channel(uint32_t rgba, int32_t index) {
    const uint32_t byte = (rgba >> ((3 - index) * 8)) & 0xffu;
    return static_cast<float>(byte) * (1.0f / 255.0f);
}

const float EXPOSURE[MAT_Count] = {1.0f, 1.9f};

} // namespace

float material_exposure(uint16_t material) {
    return material < MAT_Count ? EXPOSURE[material] : 1.0f;
}

uint32_t sprites_to_instances(const framework::graphics::SpriteList& list, const Atlas& atlas,
                              Instance* out, uint32_t max) {
    if (out == nullptr) return 0;
    uint32_t written = 0;
    for (uint32_t i = 0; i < list.count(); ++i) {
        const framework::graphics::Sprite& s = list.drawn(i);
        // Регион вне нарезки — «не рисовать», то же соглашение, что у частиц и тайлов. Номер здесь
        // приходит СНАРУЖИ (из таблицы описаний), и угол страницы вместо картинки был бы ровно тем
        // молчаливым мусором, ради которого нумерация шага B2 знает про свои границы.
        const Region* r = region_at(atlas, s.region);
        if (r == nullptr) continue;
        if (written >= max) break;
        const float e = material_exposure(s.material);
        Instance& inst = out[written++];
        inst.x = static_cast<float>(s.center.x.to_double());
        inst.y = static_cast<float>(s.center.y.to_double());
        // Полуразмер → полный: вершинный буфер игры меряет квад целиком, и множитель два здесь
        // единственный. Забыть его значило бы вдвое мельче ВСЁ, что приехало из фреймворка.
        inst.w = static_cast<float>(s.half.x.to_double()) * 2.0f;
        inst.h = static_cast<float>(s.half.y.to_double()) * 2.0f;
        inst.u0 = r->u0;
        inst.v0 = r->v0;
        inst.u1 = r->u1;
        inst.v1 = r->v1;
        inst.r = channel(s.rgba, 0) * e;
        inst.g = channel(s.rgba, 1) * e;
        inst.b = channel(s.rgba, 2) * e;
        // Прозрачность экспозицией НЕ трогается: она доля, а не яркость, и умножить её значило бы
        // сделать светящуюся частицу заодно непрозрачной.
        inst.a = channel(s.rgba, 3);
        // Направление спрайта — единичный вектор, поворот квада — угол. Тождественное направление
        // проходит без тригонометрии не ради скорости, а ради точности: `atan2(0, 1)` обязан дать
        // ровный ноль, и полагаться на это у нас нет причин.
        inst.rot = (s.dir.y.raw == 0 && s.dir.x.raw > 0)
                       ? 0.0f
                       : std::atan2(static_cast<float>(s.dir.y.to_double()),
                                    static_cast<float>(s.dir.x.to_double()));
    }
    return written;
}

} // namespace game
