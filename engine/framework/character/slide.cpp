#include "slide.hpp"

namespace framework::character {
namespace {

bool is_zero(Vec2 v) { return v.x.raw == 0 && v.y.raw == 0; }

// Снять составляющую вдоль нормали. Нормаль свипа смотрит НАРУЖУ из препятствия (`cast.hpp`),
// поэтому у вектора, направленного в поверхность, проекция отрицательна, и вычитание её
// возвращает вектор в касательную плоскость — то есть даёт скольжение, а не отскок.
Vec2 slide_along(Vec2 v, Vec2 normal) { return v - normal * dot(v, normal); }

} // namespace

SlideResult move_and_slide(const CollisionScene& s, const CharacterHull& hull, Vec2 travel,
                           fix32 max_slope, Vec2& position, Vec2& velocity) {
    SlideResult r;
    for (uint32_t pass = 0; pass < SLIDE_PASSES; ++pass) {
        if (is_zero(travel)) return r;
        SceneHit hit;
        if (!cast_nearest(s, hull, position, travel, hit)) {
            position = position + travel;
            return r;
        }
        // Отход на зазор делается ВДОЛЬ НОРМАЛИ, а не назад по пути: назад по пути он зависел бы от
        // угла подхода — при почти касательном движении отход в зазор по пути уводит на десятки
        // юнитов вбок, а нужен он строго поперёк поверхности.
        //
        // Отход БЕЗУСЛОВНЫЙ, и это стоит проговорить, потому что выглядит он как движение, которого
        // никто не разрешал. Инвариант, который он держит, ровно один: после разбора касания
        // персонаж отстоит от того, во что упёрся, на `SKIN`, а не на допуск свипа. Иначе следующий
        // свип — В ЛЮБУЮ СТОРОНУ, включая от поверхности, — вернёт долю пути ноль, и персонаж
        // залипнет; так и был потерян прыжок (`slide.hpp`).
        //
        // Проверить отход собственным свипом НЕЛЬЗЯ, и это не лень: свип отвечает нулём по той же
        // причине — стартуя в допуске у только что задетой поверхности, `cast_shape` отдаёт долю
        // ноль независимо от направления, то есть «проверка» гасила бы ровно тот отход, ради
        // которого заведена. Цена инварианта названа числом: за тик набегает не больше
        // `SLIDE_PASSES * SKIN` = 0.5 юнита, и набегает только на РАЗНЫХ касаниях, каждому из
        // которых зазор нужен самому. Что персонаж после такого отхода не оказывается вжатым во
        // встречную поверхность, пинит случай внутреннего угла в `framework_character_tunnel_test`.
        const Vec2 advance = travel * hit.fraction;
        position = position + advance + hit.normal * SKIN;
        travel = slide_along(travel - advance, hit.normal);
        velocity = slide_along(velocity, hit.normal);

        // Стена — то, что не пол и не потолок. Три ветки берутся ОДНИМ порогом (`slide.hpp`),
        // потому что склон, проходимый пробой опоры, но объявленный стеной здесь, гасил бы
        // персонажу горизонтальную скорость ровно там, где он по нему идёт.
        if (is_ceiling(hit.normal, max_slope)) r.hit_ceiling = true;
        else if (!is_floor(hit.normal, max_slope)) r.hit_wall = true;
    }
    return r;
}

bool probe_ground(const CollisionScene& s, const CharacterHull& hull, Vec2 position,
                  fix32 max_slope) {
    SceneHit hit;
    if (!cast_nearest(s, hull, position, {fix32{}, GROUND_PROBE}, hit)) return false;
    // Опора — только поверхность, на которой можно стоять. Свип вниз упирается и в стену, вдоль
    // которой персонаж скользит: считать её опорой значит выдать бесконечный прыжок от стены.
    return is_floor(hit.normal, max_slope);
}

} // namespace framework::character
