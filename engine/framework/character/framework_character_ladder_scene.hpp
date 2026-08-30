#pragma once
#include "../tilemap/query.hpp"
#include "controller.hpp"

// Раскладка гейта лестницы — отдельным файлом от утверждений, по той же границе, что у
// `framework_character_scene.hpp`: обстановка это ДАННЫЕ, а гейт — то, что о них утверждается.
// Общей сценой она не стала осознанно: шахта с односторонней крышей меняла бы траекторию голдена
// ради вопроса, который голден не задаёт.
namespace framework::character {

inline fix32 ladder_dt() { return fix32::from_int(1) / fix32::from_int(60); }
inline fix32 fx(int v) { return fix32::from_int(v); }

constexpr fix32 SHAFT_TILE = fix32::from_int(16);
constexpr tilemap::TileFlags LADDER = tilemap::TILE_LADDER;
constexpr tilemap::TileFlags ONEWAY = tilemap::TILE_SOLID | tilemap::TILE_ONEWAY;
constexpr tilemap::TileFlags LANDING = tilemap::TILE_SOLID | tilemap::TILE_ONEWAY | tilemap::TILE_LADDER;

// Шахта — колонна 5 (x от 80 до 96), ряды 2..5, то есть y от 32 до 96. Сверху ряд 1 (y от 16 до 32)
// — площадка: вся односторонняя, и ровно один её тайл, над шахтой, ещё и лестница. Снизу пол с
// верхом на 96. Персонаж — коробка 12x20, стоящий на полу держит центр на 86, стоящий на площадке —
// на 6.
constexpr fix32 SHAFT_X = fix32::from_int(88);

inline CharacterHull ladder_hull() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(fx(6), fx(10)));
    return h;
}

inline tilemap::TileGrid make_level(tilemap::TileFlags shaft) {
    tilemap::TileGrid g({fix32{}, fix32{}}, SHAFT_TILE, 12, 8);
    g.fill(0, 6, 12, 8, tilemap::TILE_SOLID);
    g.fill(3, 1, 9, 2, ONEWAY);
    g.set(5, 1, LANDING);
    for (uint32_t y = 2; y < 6; ++y) g.set(5, y, shaft);
    return g;
}

inline MoveProfile ladder_profile(uint32_t regrab) {
    MoveProfile p = default_profile();
    p.ladder_regrab_ticks = regrab;
    return p;
}

inline MoveInput held(bool up, bool down, bool jump) {
    MoveInput in;
    in.up_held = up;
    in.down_held = down;
    in.jump_held = jump;
    return in;
}

// Прогон с ОДНИМ входом на все тики. Возвращает состояние — сравнивать его после каждого прогона
// дешевле, чем заводить журнал: переходы здесь редкие и наблюдаются по краям отрезка.
struct Sim {
    tilemap::TileGrid g;
    CollisionScene s;
    CharacterHull hull = ladder_hull();
    MoveProfile p;
    MoveDerived d;
    Character c;

    Sim(tilemap::TileFlags shaft, uint32_t regrab, fix32 y)
        : g(make_level(shaft)), p(ladder_profile(regrab)), d(derive(p, ladder_dt())) {
        s.grid = &g;
        c.position = {SHAFT_X, y};
    }

    void run(MoveInput in, int ticks) {
        for (int i = 0; i < ticks; ++i) step(s, hull, p, d, in, ladder_dt(), c);
    }
    // Тот же прогон, но с ответом «побывал ли он на лестнице хоть раз». Пролёт сквозь шахту иначе
    // проверялся бы только концом пути, а прилипание на один тик с последующим срывом дало бы тот
    // же конец.
    bool run_watching(MoveInput in, int ticks) {
        bool seen = false;
        for (int i = 0; i < ticks; ++i) {
            step(s, hull, p, d, in, ladder_dt(), c);
            seen = seen || c.state == MoveState::Ladder;
        }
        return seen;
    }
};

// Ставит персонажа на опору: тяготение без ввода. Спавн прямо на грани дал бы первый тик, в
// котором опоры ещё нет, и «схватился со стояния» мерилось бы из воздуха.
inline void settle(Sim& sim) { sim.run(held(false, false, false), 60); }

// Высота покоя МЕРИТСЯ падением, а не выписывается числом. Персонаж стоит не вплотную к грани: свип
// не доезжает до неё на зазор контакта, и точная величина — свойство запроса, а не этого гейта.
// Выписанная константа сделала бы гейт ложно красным от правки зазора, к которой лестница не имеет
// отношения, — а «примерно 86» пропустило бы полтайла проникновения.
inline fix32 floor_stand() {
    Sim probe(tilemap::TILE_SOLID, 8, fx(40));
    probe.run(held(false, false, false), 60);
    return probe.c.position.y;
}
// Верх площадки на 80 юнитов выше верха пола, а контакт у них один и тот же.
inline fix32 landing_stand() { return floor_stand() - fx(80); }
} // namespace framework::character
