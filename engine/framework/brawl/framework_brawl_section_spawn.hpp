#pragma once
#include <vector>

#include "framework_brawl_check.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "depth_step.hpp"
#include "section_mode.hpp"

namespace framework::brawl::test {

constexpr int32_t NOBODY = -1;
constexpr int32_t HULL_W = HULL_HALF_W + HULL_HALF_W;

struct Landing {
    fix32 x;
    bool platform = false;
};

// Появившийся в `asked` на высоте `y` после того, как в `stand` встал другой боец.
struct SpawnOnto {
    Sections w;
    EntId id{};

    SpawnOnto(int32_t stand, int32_t asked, const std::vector<Solid>& solids = {}, int32_t y = 0) : w(solids) {
        if (stand != NOBODY) w.body(stand);
        w.step({Command{}});
        id = w.body(asked);
        w.get(id).pos.y = at(y);
        w.step({Command{}, Command{}});
    }

    Landing landing() { return {w.get(id).pos.x, w.platform_mode(id)}; }
};

inline Landing spawn_onto(int32_t stand, int32_t asked, const std::vector<Solid>& solids = {}) {
    return SpawnOnto(stand, asked, solids).landing();
}

// Сломанные фикстуры. `near_edge` — вход до В9б: занятая точка отказывала к ближнему краю секции,
// на улицу. `section_edge` — вход в секцию, но у её края: режим верный, а точка за видом. Гейт
// обязан отбить обе, каждую своим условием, иначе он не отличает правило входа от отказа к краю.
inline Landing near_edge(int32_t asked) {
    const Sections w;
    return {refusal_x(w.s.level, at(asked), scene::SectionMode::Belt), false};
}

inline Landing section_edge() { return {at(PLATFORM_X0), true}; }

inline bool beside(Landing l, int32_t asked) { return l.platform && !(at(HULL_W) < abs_fix(l.x - at(asked))); }

// Вход появившегося бойца в корпус (спека #25, В9б, решение владельца): ближайшая свободная точка
// шагом в ширину корпуса, на каждом удалении сперва вправо, только внутри секции; свободной нет —
// прежний отказ. Запрошенная `510` при стоящем в `500` пинит шаг: мельче корпуса он дал бы `518`.
inline void spawned_into_hull() {
    const Landing right = spawn_onto(500, 500);
    check(right.platform && about(right.x, 500 + HULL_W), "a fighter spawned into a hull is not moved right by one hull");
    check(beside(right, 500), "the spawn gate rejects the nearest free point");
    check(!beside(near_edge(500), 500), "the spawn gate passes the old refusal to the street");
    check(!beside(section_edge(), 500), "the spawn gate passes an entry at the far section edge");

    const Landing step = spawn_onto(500, 510);
    check(step.platform && about(step.x, 510 + HULL_W), "the spawn search does not step by the hull width");

    const Landing left = spawn_onto(500, 500, {{512, 224, 528, FLOOR}});
    check(left.platform && about(left.x, 500 - HULL_W), "a tile on the right point does not move the spawn left");

    const Landing edge = spawn_onto(792, 792);
    check(edge.platform && about(edge.x, 792 - HULL_W), "the spawn search leaves the section past its edge");

    const Landing none = spawn_onto(NOBODY, 500, {{PLATFORM_X0, 200, PLATFORM_X1, 236}});
    check(!none.platform && none.x == before(PLATFORM_X0), "a section without a free point does not refuse to its edge");

    SpawnOnto air(500, 500, {}, 20);
    const Body& b = air.w.get(air.id);
    check(air.landing().platform && about(b.pos.x, 500 + HULL_W) && at(16) < b.pos.y,
          "a fighter spawned into a hull in the air loses its height on the move");
    for (int t = 0; t < 30; ++t) air.w.step({Command{}, Command{}});
    check(grounded(air.w.get(air.id).pos) && about(air.w.get(air.id).pos.z, FLOOR), "a moved airborne spawn does not land");
}

} // namespace framework::brawl::test
