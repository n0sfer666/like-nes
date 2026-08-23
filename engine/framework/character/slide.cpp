#include "slide.hpp"

namespace framework::character {
namespace {

bool is_zero(Vec2 v) { return v.x.raw == 0 && v.y.raw == 0; }

// Снять составляющую вдоль нормали. Нормаль свипа смотрит НАРУЖУ из препятствия (`cast.hpp`),
// поэтому у вектора, направленного в поверхность, проекция отрицательна, и вычитание её
// возвращает вектор в касательную плоскость — то есть даёт скольжение, а не отскок.
Vec2 slide_along(Vec2 v, Vec2 normal) { return v - normal * dot(v, normal); }

} // namespace

SlideResult move_and_slide(const physics::World& w, const CharacterHull& hull, Vec2 travel,
                           Vec2& position, Vec2& velocity) {
    SlideResult r;
    physics::QueryFilter f;
    f.mask = hull.mask;
    for (uint32_t pass = 0; pass < SLIDE_PASSES; ++pass) {
        if (is_zero(travel)) return r;
        physics::RayHit hit;
        if (!physics::shapecast(w, hull.shape, position, fix32{}, travel, f, hit)) {
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

        // Потолок — нормаль СМОТРИТ ВНИЗ (+Y), потому что наружу из потолка это вниз. Знак здесь
        // единственное, что отличает удар головой от приземления, и записан он явно по той же
        // причине, по которой `units.hpp` записывает направление тяготения.
        if (SURFACE_NORMAL_Y < hit.normal.y) r.hit_ceiling = true;
        if (hit.normal.y < SURFACE_NORMAL_Y && -SURFACE_NORMAL_Y < hit.normal.y) r.hit_wall = true;
    }
    return r;
}

bool probe_ground(const physics::World& w, const CharacterHull& hull, Vec2 position) {
    physics::QueryFilter f;
    f.mask = hull.mask;
    physics::RayHit hit;
    if (!physics::shapecast(w, hull.shape, position, fix32{}, {fix32{}, GROUND_PROBE}, f, hit)) {
        return false;
    }
    // Опора — только поверхность, на которой можно стоять. Свип вниз упирается и в стену, вдоль
    // которой персонаж скользит: считать её опорой значит выдать бесконечный прыжок от стены.
    return hit.normal.y < -SURFACE_NORMAL_Y;
}

} // namespace framework::character
