#pragma once
#include "slide.hpp"
#include "world.hpp"

// Общая сцена гейтов вертикали 1: пол, стена, потолок и площадка с краем.
//
// Мир здесь НЕ ШАГАЕТСЯ ни разу. Персонаж не тело решателя, а статике решатель и не нужен —
// `World` в вертикали 1 играет роль контейнера геометрии, который умеют опрашивать запросы #15.
// Движущиеся платформы, ради которых мир придётся шагать, приходят с вертикалью 3.
//
// Сцена общая у трёх целей нарочно: гейты обязаны расходиться вопросом, а не обстановкой. Голден,
// окна прощения и высота прыжка, посчитанные на трёх разных раскладках, невозможно сравнивать между
// собой — «прыжок стал ниже» и «пол переехал» выглядят одинаково. Четвёртый гейт, туннелирование,
// сюда не входит осознанно: ему нужна ЗАВЕДОМО тонкая стена, а тонкая стена в общей сцене меняла бы
// траекторию голдена ради вопроса, который голден не задаёт.
namespace framework::character {

// Верх пола ровно на y = 0: считать высоту прыжка от круглого числа проще, чем от полутора
// полуразмеров, и все ожидания тестов ниже написаны от него.
constexpr fix32 FLOOR_TOP = fix32{};
constexpr fix32 HULL_HALF_W = fix32::from_int(8);
constexpr fix32 HULL_HALF_H = fix32::from_int(16);

// Край площадки, с которого персонаж сходит в гейте окна coyote.
constexpr fix32 LEDGE_RIGHT = fix32::from_int(200);

inline void add_static(physics::World& w, uint32_t key, Vec2 center, fix32 half_w, fix32 half_h) {
    physics::BodyDesc d;
    d.key = key;
    d.type = physics::BodyType::Static;
    d.shape = physics::box(half_w, half_h);
    d.position = center;
    w.add(d);
}

// Пол кончается на LEDGE_RIGHT, дальше — пропасть. Стена слева и потолок над левой половиной.
inline physics::World make_scene() {
    physics::World w(8);
    w.set_gravity({fix32{}, fix32{}});
    const fix32 half = fix32::from_int(100);
    add_static(w, 1, {LEDGE_RIGHT - fix32::from_int(300), FLOOR_TOP + half}, fix32::from_int(300),
               half);
    add_static(w, 2, {fix32::from_int(-410), FLOOR_TOP - fix32::from_int(100)}, fix32::from_int(10),
               fix32::from_int(100));
    // Потолок висит НИЖЕ целевой высоты прыжка: подвешенный выше он не участвовал бы в результате
    // вовсе, и гейт удара головой проверял бы, что персонаж до него не долетел.
    add_static(w, 3, {fix32::from_int(-300), FLOOR_TOP - fix32::from_int(58)}, fix32::from_int(100),
               fix32::from_int(10));
    return w;
}

inline CharacterHull make_hull() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(HULL_HALF_W, HULL_HALF_H));
    return h;
}

// Персонаж, стоящий на полу в `x`: центр на полувысоту выше пола, плюс зазор, в котором он и
// держится после первой же пробы опоры.
inline Character standing_at(fix32 x) {
    Character c;
    c.position = {x, FLOOR_TOP - HULL_HALF_H - SKIN};
    c.state = MoveState::Ground;
    c.on_ground = true;
    return c;
}

} // namespace framework::character
