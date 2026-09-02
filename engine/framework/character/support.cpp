#include "support.hpp"

namespace framework::character {

Vec2 support_velocity(const CollisionScene& s, physics::BodyId support) {
    if (s.world == nullptr || !support.valid()) return {};
    // Опора — ЗАПОМНЕННОЕ состояние персонажа, а сцена приезжает аргументом вызова, и связывает их
    // только вызывающий. Индекс стабилен внутри ОДНОГО мира (тела не удаляются, `world.hpp`), но
    // мир перезагруженного уровня — уже другой вектор: `World::body()` индексирует его без проверки
    // границ, то есть чтение за концом. Предусловие, которого нет в коде, — не предусловие; тот же
    // довод, которым кламплются окна в `assist.cpp`.
    if (support.index >= s.world->bodies().size()) return {};
    const physics::Body& b = s.world->body(support);
    // Предикат тот же, которым интеграция мира решает, двигать ли тело (`world.cpp`): статика
    // неподвижна по типу, замершее — по правилу покоя. Записанная в статику скорость движением не
    // становится, и перенос по ней уводил бы персонажа с НЕПОДВИЖНОЙ плиты — то есть в сторону от
    // опоры, которая никуда не ехала.
    if (b.type == physics::BodyType::Static || s.world->at_rest(support)) return {};
    return b.velocity;
}

namespace {

// Помещается ли персонаж В ТОЧКЕ. Вопрос задаётся НУЛЕВЫМ путём: «пересекаюсь ли я здесь», а не «во
// что упрусь по дороге». Свип отвечает на него первой же проверкой (`overlapped_at_start` в
// `cast.cpp`), и длина запроса в неё не входит вовсе — то есть нулевой путь тут не вырожденный
// случай, а точная формулировка вопроса.
bool fits(const CollisionScene& s, const CharacterHull& hull, Vec2 at) {
    SceneHit hit;
    return !cast_nearest(s, hull, at, {}, hit);
}

} // namespace

bool carry_by_support(const CollisionScene& s, const CharacterHull& hull, physics::BodyId support,
                      fix32 dt, Vec2& position) {
    const Vec2 carry = support_velocity(s, support) * dt;
    if (carry.x.raw == 0 && carry.y.raw == 0) return true;
    if (fits(s, hull, position + carry)) {
        position = position + carry;
        return true;
    }
    // Горизонталь, которой некуда деться, — это СКОЛЬЖЕНИЕ по опоре, а не тиски, и различие тут не
    // вкусовое. Вверх платформа персонажа ПРИЖИМАЕТ к потолку: между ними не остаётся места, и
    // отказ — единственный честный ответ. Вбок она лишь ЕДЕТ у него под ногами: сверху ничего нет,
    // стена рядом не давит, и упёршийся пассажир означает ровно то, что крыша проехала под
    // подошвами. Ящик на ленте, доехавший до стены, остаётся стоять, а не гибнет.
    //
    // Найдено третьей находкой владельческого прогона §6 (2026-09-01): плита везла персонажа,
    // прижатого к козырьку, перенос отбивался целиком, `crushed` поднимался, и образец отвечал на
    // флаг возвратом в точку появления — «сбрасывает в спавн» на ровном месте.
    if (carry.y.raw == 0) return true;
    const Vec2 lifted = {position.x, position.y + carry.y};
    if (!fits(s, hull, lifted)) return false;
    position = lifted;
    return true;
}

} // namespace framework::character
