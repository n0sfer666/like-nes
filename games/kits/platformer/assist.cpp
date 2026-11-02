#include "assist.hpp"

#include "profile.hpp"

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
                    fix32 max_shift, fix32 max_slope, Vec2& position) {
    if (max_shift.raw <= 0 || travel.y.raw >= 0) return false;
    // Окно приводится ЗДЕСЬ, а не «предполагается приведённым» — той же дисциплиной, что и профиль
    // в `step`/`derive`. Причина не в аккуратности: `fix32::operator+` НАСЫЩАЕТ, поэтому перебор
    // `shift = shift + CORNER_STEP` до `max_shift` с `raw == INT32_MAX` упирался бы в потолок типа и
    // не кончался НИКОГДА — вис с четырьмя свипами на итерацию. Главный путь спасал `sanitize`, но
    // заголовок публичный, и предусловие, которого нет в коде, есть предусловие, которого нет.
    const fix32 window = min_fix(max_shift, MAX_CORNER_CORRECTION);

    // Свип берёт ТОЛЬКО вертикальную составляющую пути. Полный путь отвечал бы на другой вопрос:
    // при разбеге вбок он упирается в стену впереди, а приём спрашивает ровно про голову.
    const Vec2 rise = {fix32{}, travel.y};
    SceneHit hit;
    if (!cast_nearest(s, hull, position, rise, hit)) return false;
    if (!is_ceiling(hit.normal, max_slope)) return false;

    const bool prefer_left = travel.x.raw < 0;
    // Счётчик ЦЕЛЫЙ: он кончается по построению, а не по тому, что сложение ещё не насытилось.
    const int32_t steps = (window / CORNER_STEP).to_int();
    for (int32_t k = 1; k <= steps; ++k) {
        const fix32 shift = CORNER_STEP * fix32::from_int(k);
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
                    fix32 max_slope, Vec2& position, GroundInfo* info) {
    if (max_distance.raw <= 0) return false;
    const Vec2 down = {fix32{}, min_fix(max_distance, MAX_GROUND_SNAP)};   // тем же приведением
    SceneHit hit;
    if (!cast_nearest(s, hull, position, down, hit)) return false;
    // Опора — только то, на чём можно стоять, тем же порогом, что и у пробы (`slide.cpp`). Свип
    // вниз упирается и в стену, вдоль которой персонаж скользит, и притянуть себя к ней значило бы
    // выдать бесконечный спуск по стене за «спуск по склону».
    if (!is_floor(hit.normal, max_slope)) return false;
    position = position + down * hit.fraction + hit.normal * SKIN;
    if (info != nullptr) *info = {hit.oneway, hit.body};
    return true;
}

} // namespace framework::character
