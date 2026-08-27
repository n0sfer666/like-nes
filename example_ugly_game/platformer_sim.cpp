#include "platformer_sim.hpp"

#include "trajectory.hpp"

#include <cstdio>

// Маршрут по уровню — ТАБЛИЦЕЙ, а не кодом с условиями: скрипт, который смотрит на состояние и
// решает, что нажать, перестаёт быть записью ввода и становится вторым контроллером. Разошёлся бы
// такой прогон — и непонятно, поехал контроллер или поехал скрипт.
//
// Каждая полоса названа приёмом, который она проверяет, а не «идём вправо»: числа тиков подобраны
// трассой (`--trace`), и без имени первая же правка карты превратила бы таблицу в набор магических
// чисел, про которые известно только то, что с ними хеш сходится. Имена английские и ASCII, потому
// что их ПЕЧАТАЕТ трасса: консоль Windows не UTF-8, и гейт `tree_invariants.sh` держит это правило
// для всего рантайм-вывода — комментарии вокруг остаются русскими.
namespace platformer {
namespace {

struct Beat {
    uint32_t ticks;
    int move_x;     // -1 | 0 | +1
    bool jump;
    bool down;
    const char* what;
};

// Раскладка уровня, от которой считаны числа (`assets/tilemap.txt`, тайл 16, +Y вниз):
//   пол y=224 во всю ширину; нижняя односторонняя x 80..160 верхом y=176; верхняя x 48..128
//   верхом y=128; холм 45° x 224..352 с плато y=192; ступень x 448..496 верхом y=176;
//   козырёк x 592..640 низом y=176; правая стена x=624. Платформа ходит 520..568 по центру.
constexpr Beat SCRIPT[] = {
    {22, +1, false, false, "run right along the floor, under the lower one-way"},
    {10, 0, false, false, "brake: the next jump is vertical, so he must not overshoot the overlap"},
    {44, 0, true, false, "jump THROUGH the lower one-way from below and land on top of it"},
    {8, -1, false, false, "step left, into the X overlap of the two platforms"},
    {6, 0, false, false, "stop"},
    {44, 0, true, false, "jump onto the upper one-way"},
    {8, 0, false, false, "stand: the button must release, or the next edge never happens"},
    {3, 0, true, true, "down+jump: drop through the upper one-way"},
    {26, 0, false, false, "fall onto the lower one-way"},
    {3, 0, true, true, "down+jump: drop through the lower one-way"},
    {26, 0, false, false, "fall onto the floor"},
    {20, +1, false, false, "run right along the floor, towards the foot of the hill"},
    {20, +1, false, false, "climb the 45-degree slope and the plateau: a slope acts as FLOOR"},
    {10, +1, false, false, "walk the slope down: the ground snap must keep the support"},
    {16, +1, false, false, "run the floor up to the step and hit its wall"},
    {10, 0, true, false, "standing jump along the wall: 64 units of jump against 48 of step"},
    {6, +1, true, false, "carry over the edge of the step"},
    {11, 0, true, false, "land on the step"},
    {14, +1, false, false, "step onto the plate while its edge still stands at the step's edge"},
    {40, 0, false, false, "ride: the support's motion carries him with no press at all"},
    {13, +1, false, false, "walk right on the roof: the overhang's face does NOT let the rider past"},
    {11, 0, false, false, "the plate slid out from under him to the left - fall into the pocket"},
    {6, +1, false, false, "run along the pocket floor, in under the overhang"},
    {20, 0, true, false, "jump under the overhang: 48 of clearance against 32 of height - head hit"},
    {20, +1, false, false, "push into the right wall"},
};

ch::MoveInput input_of(const Beat& b) {
    ch::MoveInput in;
    in.move_x = fix32::from_int(b.move_x);
    in.jump_held = b.jump;
    in.down_held = b.down;
    return in;
}

double to_double(fix32 v) { return static_cast<double>(v.raw) / 65536.0; }

} // namespace

RunResult run_script(Stage& st, bool trace) {
    ch::TrajectoryHash hash;
    RunResult r;

    bool prev_on_oneway = false;
    Vec2 prev_pos = st.hero.position;

    for (const Beat& b : SCRIPT) {
        const ch::MoveInput in = input_of(b);
        if (trace) std::printf("-- %s\n", b.what);
        for (uint32_t i = 0; i < b.ticks; ++i) {
            step_stage(st, in);
            hash.feed(st.hero);
            ++r.ticks;

            const tm::TileFlags under = ground_tile(st);
            if (under & tm::TILE_SLOPE) r.seen.walked_slope = true;
            const bool on_oneway = (under & tm::TILE_ONEWAY) != 0;
            if (on_oneway) r.seen.stood_oneway = true;

            // Подъём СКВОЗЬ площадку: подошва внутри одностороннего тайла, а сам он летит вверх.
            // Спрашивается именно подошва, а не центр: персонаж ростом в два тайла, и центр
            // проходит сквозь площадку кадром позже — по нему приём «прошёл снизу» не отличить от
            // «стоит рядом».
            const Vec2 feet = {st.hero.position.x, st.hero.position.y + HULL_HALF_H};
            if (st.hero.velocity.y.raw < 0 && (tile_at_point(st, feet) & tm::TILE_ONEWAY))
                r.seen.rose_through = true;

            // Спуск сквозь неё: прошлый тик стоял на односторонней, этот — уже падает.
            if (prev_on_oneway && !st.hero.on_ground && st.hero.velocity.y.raw > 0)
                r.seen.dropped_through = true;

            if (st.hero.on_ground && st.hero.support.index == st.lift.index) {
                r.seen.rode_lift = true;
                // ВЕЗЛА, а не «стоял на ней»: без ввода и без собственной скорости персонаж сдвинулся
                // по X — значит, движение опоры до него доехало. Опора, которую забыли перенести,
                // проходит проверку выше и валится здесь.
                if (b.move_x == 0 && st.hero.position.x.raw != prev_pos.x.raw)
                    r.seen.carried = true;
            }

            if (st.hero.hit_ceiling) r.seen.hit_ceiling = true;
            if (st.hero.hit_wall) r.seen.hit_wall = true;
            if (st.hero.crushed) r.seen.crushed = true;

            if (trace)
                std::printf("t%4u pos %8.3f %8.3f vel %8.3f %8.3f %s%s%s%s tile %u lift %7.3f\n",
                            r.ticks, to_double(st.hero.position.x), to_double(st.hero.position.y),
                            to_double(st.hero.velocity.x), to_double(st.hero.velocity.y),
                            st.hero.on_ground ? "G" : "a", st.hero.hit_wall ? "W" : "-",
                            st.hero.hit_ceiling ? "C" : "-", st.hero.crushed ? "X" : "-",
                            static_cast<unsigned>(under),
                            to_double(st.world.body(st.lift).position.x));

            prev_on_oneway = on_oneway;
            prev_pos = st.hero.position;
        }
    }

    r.hash = hash.value;
    r.last_position = st.hero.position;
    return r;
}

} // namespace platformer
