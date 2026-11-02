#pragma once
#include "collision.hpp"
#include "profile.hpp"

namespace framework::brawl {

// Мир платформенных секций: тела в platform ведёт контроллер #16 по своей сцене столкновений, а не
// `DepthFloor`. Профиль один на всех: второго потребителя, которому нужен свой, у фреймворка нет.
// `dead_y` — мёртвая зона камеры по экранному `y`: без неё вид качался бы на каждом прыжке.
struct PlatformWorld {
    character::CollisionScene scene;
    character::CharacterHull hull;
    character::MoveProfile profile;
    character::MoveDerived derived;
    fix32 dt;
    fix32 dead_y;
};

} // namespace framework::brawl
