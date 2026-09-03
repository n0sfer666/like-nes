#pragma once

#include "units.hpp"
#include "world.hpp"

// Сцена снимка состояния (вертикаль 1 спеки #22) — башня из трёх ящиков, которая ЗАМИРАЕТ, и
// снаряд, который её будит.
//
// Сцена подобрана не «чтобы было движение», а по составу состояния, и это измерено. Первая попытка
// гнала башню из `framework_physics_stack_scene.hpp`: она не замирает ни разу за 400 тиков
// (`frozen=0`, `recalled=0` на всём прогоне), поэтому снимок, ВЫБРАСЫВАЮЩИЙ правило покоя, связность
// островов или диффер контактов, проходил её молча — три поля состояния оставались непроверенными, а
// гейт выглядел зелёным.
//
// Здесь на каждое из них есть кадр, где оно решает:
//   t≈20  башня замирает — заполняется `RestTracker` (якоря, счётчики) и включается кеш (`recalled=3`);
//   t≈78  снаряд доезжает и БУДИТ остров — в дело идут связность (`Islands::root`, читается в начале
//         шага, ДО пересборки) и диффер контактов, дающий события входа;
//   t≈130 остров замирает заново — уже в другой раскладке.
// Снимать поэтому осмысленно и до удара, и после: раскладки покоя в этих двух точках разные.
namespace framework::physics::snapshot_scene {

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr uint32_t BOXES = 3;
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
// Трение обязательно: без него башня не замирает и за 30 секунд (измерено в сцене пробуждения), а
// сцена без замирания не проверяет здесь ровно то, ради чего написана.
constexpr fix32 GRIP = fix32::from_float(0.6);
// Зона-триггер на пути снаряда. Стоит она там, где снаряд ЕЩЁ в ней на первой точке снятия и УЖЕ не
// в ней в конце прогона: список триггеров иначе пуст в обеих точках, и снимок, его потерявший,
// проходил бы молча — сравнивать было бы нечего.
constexpr fix32 ZONE_X = fix32::from_int(-53);
constexpr fix32 ZONE_Y = fix32::from_int(184);

// Тик замирания, удара и повторного замирания — измерены, а не задуманы. Тест утверждает их, потому
// что сцена, тихо переставшая замирать, унесла бы с собой смысл трёх утверждений сразу.
constexpr uint32_t SETTLES_BY = 30;
constexpr uint32_t TAKE_BEFORE_HIT = 40;
// Точка снятия ПОСРЕДИ удара. Она не украшение: до неё все точки снятия давали ту же раскладку, что
// и конец прогона (замерший остров, четыре контакта, три пары из кеша), и снимок, потерявший списки
// контактов, счётчики или диффер, сверку проходил молча — сравнивать было не с чем. Здесь остров
// разбужен, контактов пять, из кеша не взято ничего: с концом прогона не совпадает НИЧЕГО.
constexpr uint32_t TAKE_AT_HIT = 80;
constexpr uint32_t HITS_BY = 100;
constexpr uint32_t TAKE_AFTER_HIT = 200;
constexpr uint32_t RUN_TO = 400;

inline void build(World& w) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, GRIP};
    w.add(floor);

    for (uint32_t i = 0; i < BOXES; ++i) {
        BodyDesc b;
        b.key = 10 + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32{}, FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16)};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, GRIP};
        w.add(b);
    }

    // Снаряд скользит по полу БЕЗ трения: с трением башни он не доезжает, и кадра пробуждения в
    // сцене не случается вовсе.
    BodyDesc shot;
    shot.key = 20;
    shot.shape = box(HALF, HALF);
    shot.position = {fix32::from_int(-100), FLOOR_TOP - HALF};
    shot.velocity = {fix32::from_int(70), fix32{}};
    shot.mass = fix32::from_int(4);
    shot.material = {fix32{}, fix32{}};
    w.add(shot);

    BodyDesc zone;
    zone.key = 30;
    zone.type = BodyType::Static;
    zone.trigger = true;
    zone.shape = box(fix32::from_int(5), fix32::from_int(6));
    zone.position = {ZONE_X, ZONE_Y};
    w.add(zone);
}

inline uint32_t frozen_count(const World& w) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(w.bodies().size()); ++i) {
        if (w.at_rest(BodyId{i})) ++n;
    }
    return n;
}

} // namespace framework::physics::snapshot_scene
