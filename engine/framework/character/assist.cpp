#include "assist.hpp"

namespace framework::character {
namespace {

// Свободен ли боковой сдвиг. Именно СВИПОМ, а не «пусто ли в конце»: сдвиг на четыре юнита сквозь
// столб в один тайл кончается в пустоте, и проверка конечной точки разрешала бы ровно то
// прохождение сквозь угол, ради обхода которого приём заведён.
bool side_is_free(const CollisionScene& s, const CharacterHull& hull, Vec2 position, fix32 dx) {
    SceneHit hit;
    return !cast_nearest(s, hull, position, {dx, fix32{}}, hit);
}

} // namespace

bool corner_correct(const CollisionScene& s, const CharacterHull& hull, Vec2 travel,
                    fix32 max_shift, Vec2& position) {
    if (max_shift.raw <= 0 || travel.y.raw >= 0) return false;

    // Свип берёт ТОЛЬКО вертикальную составляющую пути. Полный путь отвечал бы на другой вопрос:
    // при разбеге вбок он упирается в стену впереди, а приём спрашивает ровно про голову.
    const Vec2 rise = {fix32{}, travel.y};
    SceneHit hit;
    if (!cast_nearest(s, hull, position, rise, hit)) return false;
    if (!(SURFACE_NORMAL_Y < hit.normal.y)) return false;

    const bool prefer_left = travel.x.raw < 0;
    for (fix32 shift = CORNER_STEP; !(max_shift < shift); shift = shift + CORNER_STEP) {
        for (int i = 0; i < 2; ++i) {
            const bool left = (i == 0) == prefer_left;
            const fix32 dx = left ? -shift : shift;
            if (!side_is_free(s, hull, position, dx)) continue;
            const Vec2 shifted = {position.x + dx, position.y};
            SceneHit above;
            if (cast_nearest(s, hull, shifted, rise, above)) continue;
            position = shifted;
            return true;
        }
    }
    return false;
}

bool snap_to_ground(const CollisionScene& s, const CharacterHull& hull, fix32 max_distance,
                    Vec2& position) {
    if (max_distance.raw <= 0) return false;
    const Vec2 down = {fix32{}, max_distance};
    SceneHit hit;
    if (!cast_nearest(s, hull, position, down, hit)) return false;
    // Опора — только то, на чём можно стоять, тем же порогом, что и у пробы (`slide.cpp`). Свип
    // вниз упирается и в стену, вдоль которой персонаж скользит, и притянуть себя к ней значило бы
    // выдать бесконечный спуск по стене за «спуск по склону».
    if (!(hit.normal.y < -SURFACE_NORMAL_Y)) return false;
    position = position + down * hit.fraction + hit.normal * SKIN;
    return true;
}

} // namespace framework::character
