#pragma once
#include <utility>

#include "controller.hpp"

// РАСКЛАДКА гейта движущейся платформы: одно тело-платформа, необязательный потолок над ней и один
// прогон в порядке кадра. Своим заголовком, а не первой сотней строк гейта, по той же границе, что
// у общей сцены персонажа: здесь обстановка, там утверждения. Гейт, выросший вместе с обстановкой,
// упирался бы в жёсткий потолок бюджета длины ровно тогда, когда к нему добавится случай.
//
// Платформа тут — ТЕЛО и только тело: у `tilemap::TileGrid` начало координат лишь читается,
// сдвинуть слой тайлов нечем, поэтому сетки в сцене нет вовсе.
namespace framework::character {

inline fix32 tick_dt() { return fix32::from_int(1) / fix32::from_int(60); }

constexpr fix32 PLATFORM_TOP = fix32::from_int(100);
constexpr fix32 PLATFORM_HALF_W = fix32::from_int(48);
constexpr fix32 PLATFORM_HALF_H = fix32::from_int(8);
constexpr fix32 RIDER_HALF_W = fix32::from_int(6);
constexpr fix32 RIDER_HALF_H = fix32::from_int(10);

// Скорости названы задачей, а не числом: за тик при 60 Гц это 2, 1, 4 и 10 юнитов.
constexpr fix32 CARRY_V = fix32::from_int(120);
constexpr fix32 RISE_V = fix32::from_int(60);   // медленно нарочно: платформа не догоняет прыжок
constexpr fix32 CRUSH_V = fix32::from_int(240);
constexpr fix32 FALL_V = fix32::from_int(600);  // 10 юнитов за тик — ГЛУБЖЕ окна притяжения (8)

// Низ потолка ровно на 60: над макушкой стоящего (79.875) остаётся 19.875 юнита.
constexpr fix32 CEILING_BOTTOM = fix32::from_int(60);

inline CharacterHull make_rider() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(RIDER_HALF_W, RIDER_HALF_H));
    return h;
}

struct Stage {
    physics::World world;
    physics::BodyId platform;

    // Указатель отдаёт метод, а не поле: сцена возвращается по значению, и указатель, взятый до
    // перемещения, смотрел бы в прежнее место.
    CollisionScene view() const { return {&world, nullptr}; }
};

inline Stage make_stage(physics::BodyType type, Vec2 velocity, bool ceiling) {
    physics::World w(8);
    w.set_gravity({fix32{}, fix32{}});
    physics::BodyDesc d;
    d.key = 1;
    d.type = type;
    d.shape = physics::box(PLATFORM_HALF_W, PLATFORM_HALF_H);
    d.position = {fix32{}, PLATFORM_TOP + PLATFORM_HALF_H};
    d.velocity = velocity;
    const physics::BodyId id = w.add(d);
    if (ceiling) {
        physics::BodyDesc c;
        c.key = 2;
        c.type = physics::BodyType::Static;
        c.shape = physics::box(PLATFORM_HALF_W, fix32::from_int(20));
        c.position = {fix32{}, CEILING_BOTTOM - fix32::from_int(20)};
        w.add(c);
    }
    return {std::move(w), id};
}

// Персонаж, стоящий на платформе в `x`: центр на полувысоту выше её верха, плюс зазор, в котором
// он и держится после первой же пробы опоры.
inline Character rider_at(fix32 x) {
    Character c;
    c.position = {x, PLATFORM_TOP - RIDER_HALF_H - SKIN};
    c.on_ground = true;
    c.state = MoveState::Ground;
    return c;
}

// Прогревочный тик БЕЗ шага мира: опору запоминает шаг 8, и до неё переносить персонажа нечем. Так
// же начинается и настоящий кадр — на первом персонаж стоит на ещё не сдвинувшейся платформе. Ввод
// пустой намеренно: прыжок ловится ФРОНТОМ, и зажми мы кнопку тут, он израсходовался бы на прогрев.
inline void acquire_support(const Stage& st, const MoveProfile& p, const MoveDerived& d,
                            Character& c) {
    step(st.view(), make_rider(), p, d, MoveInput{}, tick_dt(), c);
}

// Персонаж висит СБОКУ от платформы, а не над ней: центр внутри её полосы по высоте (100..116),
// левый бок в шести юнитах правее её правого края. Шесть — три тика хода `CARRY_V`: прогон обязан
// начаться с чистого воздуха, иначе первое же касание случилось бы до первого шага мира и гейт
// проверял бы разбор стартового перекрытия, а не встречу.
constexpr fix32 HANG_X = fix32::from_int(60);
// Тот же бок, но вплотную: падающий персонаж выпадает из полосы платформы за считанные тики, и три
// тика чистого воздуха съели бы всю встречу целиком.
constexpr fix32 TOUCH_X = PLATFORM_HALF_W + RIDER_HALF_W;
constexpr fix32 HANG_Y = fix32::from_int(108);
// Стена справа от персонажа: между её левым краем и его правым боком — те же шесть юнитов, то есть
// три тика сноса. Ключ 2, потому что 1 занят платформой, а ничью долей пути физика разводит ключом.
inline void add_wall(Stage& st, fix32 x) {
    physics::BodyDesc w;
    w.key = 2;
    w.type = physics::BodyType::Static;
    w.shape = physics::box(fix32::from_int(8), fix32::from_int(20));
    w.position = {x, HANG_Y};
    st.world.add(w);
}
// Свидетель, которого персонаж КАСАЕТСЯ, но который никуда не едет: неподвижная крышка над
// макушкой, ключом МЕНЬШЕ платформы. Ключ не подгонка, а сам дефект: свип отвечает одним ближайшим,
// на нулевом пути ближайших столько же, сколько касаний, и разводит их ключ — то есть раскладка.
// Первая версия сноса спрашивала мир ровно так и получала крышку: та не едет, сносить нечем, и
// платформа проезжала сквозь персонажа при живом гейте выше.
inline void add_lid(Stage& st) {
    physics::BodyDesc l;
    l.key = 0;
    l.type = physics::BodyType::Static;
    l.shape = physics::box(fix32::from_int(48), fix32::from_int(20));
    l.position = {fix32::from_int(100), HANG_Y - RIDER_HALF_H - fix32::from_int(20)};
    st.world.add(l);
}

// Столб НА ПУТИ ПАССАЖИРА: стоит над крышей платформы (60..100 по высоте) и в её ход по
// горизонтали не лезет вовсе — низ столба ровно на верхе плиты, и та проезжает под ним. Стоящий на
// крыше персонаж занимает 79.875..99.875, то есть упирается в столб БОКОМ, а не макушкой: между ним
// и столбом нет ничего, чем можно сдавить, и остановленный им перенос — скольжение по опоре, а не
// тиски. Ключ 3: 1 у платформы, 2 у стены сноса, а ничью долей пути физика разводит ключом.
constexpr fix32 PILLAR_HALF_W = fix32::from_int(8);
constexpr fix32 PILLAR_X = fix32::from_int(30);
inline void add_pillar(Stage& st) {
    physics::BodyDesc c;
    c.key = 3;
    c.type = physics::BodyType::Static;
    c.shape = physics::box(PILLAR_HALF_W, fix32::from_int(20));
    c.position = {PILLAR_X, PLATFORM_TOP - fix32::from_int(20)};
    st.world.add(c);
}

struct Run {
    Vec2 moved;     // путь ПЕРСОНАЖА за считанные тики
    Vec2 platform;  // путь его ОПОРЫ за те же тики
    fix32 peak;     // самая высокая точка относительно старта
    bool kept_ground = true;
    bool crushed = false;
};

// Прогон: `ticks` тиков в порядке «мир, потом персонаж» — тот самый порядок кадра, который решение
// владельца записало контрактом и которым перенос вообще имеет смысл.
inline Run go(Stage& st, fix32 move_x, bool jump, uint32_t ticks) {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    Character c = rider_at(fix32{});
    acquire_support(st, p, d, c);
    const Vec2 from = c.position;
    const Vec2 plat_from = st.world.body(st.platform).position;
    Run r;
    for (uint32_t t = 0; t < ticks; ++t) {
        st.world.step(tick_dt());
        MoveInput in;
        in.move_x = move_x;
        in.jump_held = jump;
        step(st.view(), make_rider(), p, d, in, tick_dt(), c);
        r.peak = max_fix(r.peak, from.y - c.position.y);
        r.kept_ground = r.kept_ground && c.on_ground;
        r.crushed = r.crushed || c.crushed;
    }
    r.moved = c.position - from;
    r.platform = st.world.body(st.platform).position - plat_from;
    return r;
}

} // namespace framework::character
