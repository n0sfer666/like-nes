#include <cstdio>
#include <vector>

#include "platform_args.hpp"
#include "query.hpp"

// Инварианты спеки #16, которые проверяются не одним ответом, а ОТСУТСТВИЕМ ответа на целом классе
// сцен: «не залипать в стыке тайлов» (инвариант 3) и «не проходить сквозь сплошное» (инвариант 2).
//
// Утверждение о пустоте вакуумно само по себе: запрос, не находящий ничего никогда, проходит его
// идеально. Поэтому у каждого здесь ПАРА — сцена, где ответа быть не должно, и сцена из тех же
// тайлов, где он обязан быть, с точностью до доли и нормали.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::tilemap;

fix32 fx(int v) { return fix32::from_int(v); }

constexpr fix32 SKIN = fix32::from_raw(8192);

TileGrid flat_floor() {
    TileGrid g({fix32{}, fix32{}}, fx(16), 12, 12);
    g.fill(0, 6, 12, 8, TILE_SOLID);
    return g;
}

// Инвариант 3, пара сцен. Первая — плоский пол: боковая грань каждого тайла закрыта соседом, и
// свип вдоль пола не смеет останавливаться ни на одной из них. Вторая — тот же пол со ступенью:
// грань той же формы, но открытая, и свип обязан встать на ней с названной долей.
//
// Пара держится на том, что различаются сцены ОДНИМ тайлом. Слепой запрос (сломанное окно, неверная
// маска, потерянный вызов свипа) отвечает пустотой на обеих и красит вторую половину.
void test_seam_versus_step() {
    const TileGrid flat = flat_floor();
    TileGrid step = flat_floor();
    step.set(7, 5, TILE_SOLID);

    TileFilter f;
    TileHit h;
    const physics::Shape body = physics::box(fx(6), fx(12));
    const Vec2 rest{fx(80), fx(96) - SKIN - fx(12)};

    check(!shapecast(flat, body, rest, fix32{}, {fx(48), fix32{}}, f, h),
          "walking along a flat tiled floor snags on nothing");
    check(shapecast(step, body, rest, fix32{}, {fx(48), fix32{}}, f, h),
          "the same walk stops at a step made of one more tile");
    check(h.x == 7 && h.y == 5 && h.normal == Vec2{fx(-1), fix32{}},
          "on the step's left face, outward");
    check(h.fraction == fix32::from_int(26) / fix32::from_int(48),
          "at the fraction its position dictates");
}

// Тот же инвариант классом, а не точкой: ни один свип вдоль плоского пола не смеет вернуть осевую
// БОКОВУЮ нормаль. Сцены выровнены по сетке нарочно — зацеп за стык это явление границ, и случайные
// координаты Q16.16 в них почти не попадают.
void test_seam_across_the_grid() {
    const TileGrid g = flat_floor();
    TileFilter f;
    const physics::Shape body = physics::box(fx(6), fx(12));
    const int32_t gaps[] = {-8192, -4096, -2048, 0, 1024};        // от −1/8 до +1/64 в сыром Q16.16
    const int32_t offs[] = {0, 16384, 32768, 65536, 196608, 524288, 557056};
    const int32_t lens[] = {1, 3, 5, 16, 17, 48, 64};

    int scenes = 0, side_normals = 0, hits = 0;
    for (int32_t gap : gaps)
        for (int32_t off : offs)
            for (int32_t len : lens)
                for (int32_t dir : {1, -1}) {
                    const Vec2 at{fx(80) + fix32::from_raw(off),
                                  fx(96) + fix32::from_raw(gap) - fx(12)};
                    TileHit h;
                    ++scenes;
                    if (!shapecast(g, body, at, fix32{}, {fx(dir * len), fix32{}}, f, h)) continue;
                    ++hits;
                    if (h.normal.y.raw == 0 && h.normal.x.raw != 0) ++side_normals;
                }

    std::printf("  flat floor: %d scenes, %d hits, %d side normals\n", scenes, hits, side_normals);
    check(scenes == 490, "the sweep covers the whole table of gaps, offsets and lengths");
    check(side_normals == 0, "and not one scene answers with a sideways normal");
    // Половина сцен обязана КАСАТЬСЯ пола: без этого ноль боковых нормалей выше означал бы, что
    // запрос не видит пола вовсе, а не что стыка в нём нет.
    check(hits > scenes / 3, "while the floor itself is seen in a good share of them");
}

// Инвариант 2: путь, кончающийся внутри сплошного участка, обязан быть остановлен. Проверяется
// перебором направлений вокруг зонда, и счётчик «сколько путей вообще заканчивались внутри» стоит
// утверждением — набор, где таких путей ноль, проходит проверку молча и не проверяет ничего.
void test_no_pass_through() {
    TileGrid g({fix32{}, fix32{}}, fx(16), 12, 12);
    g.fill(2, 6, 10, 10, TILE_SOLID);            // сплошная плита 8x4 тайла
    TileFilter f;
    const physics::Shape body = physics::box(fx(4), fx(4));
    std::vector<TileOverlap> ov;

    int ended_inside = 0, unstopped = 0;
    for (int32_t a = 0; a < 64; ++a) {
        // Зонд по кругу над плитой, путь — насквозь через неё, длиной больше её толщины.
        const fix32 sx = fx(40) + fix32::from_raw(a * 4096);
        const Vec2 at{sx, fx(40)};
        for (int32_t len : {64, 80, 96, 112}) {
            const Vec2 travel{fix32::from_raw((a - 32) * 2048), fx(len)};
            overlap_shape(g, body, at, fix32{}, f, ov);
            if (!ov.empty()) continue;
            TileHit h;
            const bool hit = shapecast(g, body, at, fix32{}, travel, f, h);
            overlap_shape(g, body, {at.x + travel.x, at.y + travel.y}, fix32{}, f, ov);
            if (ov.empty()) continue;
            ++ended_inside;
            if (!hit) ++unstopped;
        }
    }

    std::printf("  paths ending inside the slab: %d, unstopped: %d\n", ended_inside, unstopped);
    check(ended_inside > 64, "the table really drives paths into the slab");
    check(unstopped == 0, "and every one of them is stopped before it gets there");
}

// Решение владельца 2026-08-23: тело, оказавшееся ЦЕЛИКОМ внутри тайлов, получает долю ноль, а не
// «путь свободен». Контроль — то же тело на тайл выше: там ответ обязан быть настоящим касанием с
// ненулевой долей, иначе «доля ноль» ниже означало бы, что запрос отвечает ею всегда.
void test_embedded_body() {
    TileGrid g({fix32{}, fix32{}}, fx(16), 12, 12);
    g.fill(2, 6, 10, 10, TILE_SOLID);
    TileFilter f;
    TileHit h;
    const physics::Shape body = physics::box(fx(4), fx(4));

    check(shapecast(g, body, {fx(80), fx(120)}, fix32{}, {fx(32), fix32{}}, f, h),
          "a body sunk entirely inside the slab is not told the path is free");
    check(h.fraction.raw == 0, "it is told it is already in contact");
    check(h.normal.x.raw != 0 || h.normal.y.raw != 0, "with some face named, not a zero normal");

    TileHit above;
    check(shapecast(g, body, {fx(80), fx(80)}, fix32{}, {fix32{}, fx(32)}, f, above),
          "the same body a tile higher reaches the slab");
    check(above.fraction.raw > 0, "after a real part of its path");
    check(above.normal == Vec2{fix32{}, fx(-1)}, "landing on the top face");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework tilemap seam gate\n");
    test_seam_versus_step();
    test_seam_across_the_grid();
    test_no_pass_through();
    test_embedded_body();
    std::printf("framework-tilemap-seam: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
