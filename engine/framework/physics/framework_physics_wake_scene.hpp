#pragma once

#include "world.hpp"

// Сцена пробуждения замершего острова — башня из трёх ящиков на полу — и мерки к ней, общие для ДВУХ
// целей: три двери, через которые остров обязан ожить, и неконстантная ручка тела, у которой обе
// стороны контракта были дефектами.
//
// Замирание останавливает остров целиком: ни тяготения, ни интеграции, ни решателя. Значит он не
// читает НИЧЕГО из поменявшегося вокруг, и закрытая дверь означает не «сработает позже», а не
// сработает никогда — будить его больше некому. Выглядит это не отказом, а сломанной физикой: ящик
// висит в воздухе там, где из-под него убрали пол.
//
// Трение здесь не декорация: без него башня не замирает и за 30 секунд (измерено), а мерить
// пробуждение на незамёрзшей сцене значит мерить пустоту — гейт зеленел бы и с наглухо выключенным
// пробуждением. Поэтому `settle` возвращает КАДР замирания, а не `void`: обе цели начинают с
// утверждения, что замирать вообще было чему.
namespace framework::physics::wake {

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
constexpr uint32_t BOXES = 3;
// Окно — граница отказа, а не ожидание: башня замирает за два десятка кадров, взято вдесятеро шире.
constexpr uint32_t WINDOW = 30 * 60;
constexpr uint32_t FLOOR = 0;
constexpr uint32_t TOP = BOXES;
constexpr fix32 GRIP = fix32::from_float(0.6);

inline void build(World& w, fix32 friction) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, friction};
    w.add(floor);

    for (uint32_t i = 0; i < BOXES; ++i) {
        BodyDesc b;
        b.key = 10 + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32{}, FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16)};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, friction};
        w.add(b);
    }
}

inline bool tower_at_rest(const World& w) {
    for (uint32_t i = 1; i <= BOXES; ++i) {
        if (!w.at_rest(BodyId{i})) return false;
    }
    return true;
}

// Кадров до замирания башни, 0 — не замерла в окне.
inline uint32_t settle(World& w) {
    for (uint32_t i = 0; i < WINDOW; ++i) {
        w.step(DT);
        if (tower_at_rest(w)) return i + 1;
    }
    return 0;
}

inline fix32 top_y(const World& w) { return w.bodies()[TOP].position.y; }

} // namespace framework::physics::wake
