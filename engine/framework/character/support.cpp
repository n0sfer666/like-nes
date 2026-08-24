#include "support.hpp"

namespace framework::character {

Vec2 support_velocity(const CollisionScene& s, physics::BodyId support) {
    if (s.world == nullptr || !support.valid()) return {};
    const physics::Body& b = s.world->body(support);
    // Предикат тот же, которым интеграция мира решает, двигать ли тело (`world.cpp`): статика
    // неподвижна по типу, замершее — по правилу покоя. Записанная в статику скорость движением не
    // становится, и перенос по ней уводил бы персонажа с НЕПОДВИЖНОЙ плиты — то есть в сторону от
    // опоры, которая никуда не ехала.
    if (b.type == physics::BodyType::Static || s.world->at_rest(support)) return {};
    return b.velocity;
}

bool carry_by_support(const CollisionScene& s, const CharacterHull& hull, physics::BodyId support,
                      fix32 dt, Vec2& position) {
    const Vec2 carry = support_velocity(s, support) * dt;
    if (carry.x.raw == 0 && carry.y.raw == 0) return true;
    const Vec2 moved = position + carry;
    // Вопрос задаётся НУЛЕВЫМ путём: «пересекаюсь ли я здесь», а не «во что упрусь по дороге».
    // Свип отвечает на него первой же проверкой (`overlapped_at_start` в `cast.cpp`), и длина
    // запроса в неё не входит вовсе — то есть нулевой путь тут не вырожденный случай, а точная
    // формулировка вопроса.
    SceneHit hit;
    if (cast_nearest(s, hull, moved, {}, hit)) return false;
    position = moved;
    return true;
}

} // namespace framework::character
