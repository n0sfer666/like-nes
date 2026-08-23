#include <cstdio>
#include <vector>

#include "hash_mix.hpp"
#include "platform_args.hpp"
#include "query.hpp"

// Гейт шага A вертикали 2: три запроса к сетке против ответов, посчитанных ИЗ РАСКЛАДКИ, и голден
// свёртки всех ответов сверху. Числа в утверждениях выведены из координат сцены (тайл 16, пол
// сверху на y = 96), а не списаны с прогона: голден без такого утверждения пинит любое поведение,
// в том числе сломанное.
//
// Инварианты стыка и непроницаемости живут в `framework_tilemap_seam_test` — там у них своя сцена
// и свой позитивный контроль.
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

constexpr fix32 SKIN = fix32::from_raw(8192);   // 1/8 юнита — зазор, который держит контроллер

// Сцена гейта: пол двумя рядами (верх ровно на y = 96), ступень на ряд выше, колонна стены до пола
// и одинокий тайл в воздухе. Каждый элемент отвечает на свой вопрос, и все четыре нужны: пол даёт
// верхнюю грань, ступень и колонна — боковую, одинокий тайл — грань, у которой нет соседей вовсе.
TileGrid scene() {
    TileGrid g({fix32{}, fix32{}}, fx(16), 12, 12);
    g.fill(0, 6, 12, 8, TILE_SOLID);
    g.set(7, 5, TILE_SOLID);
    g.fill(10, 2, 11, 6, TILE_SOLID);
    g.set(2, 2, TILE_SOLID);
    return g;
}

uint64_t golden = physics::FNV_OFFSET;

void feed(bool hit, const TileHit& h) {
    physics::mix(golden, hit ? 1u : 0u);
    if (!hit) return;
    physics::mix(golden, static_cast<uint32_t>(h.x));
    physics::mix(golden, static_cast<uint32_t>(h.y));
    physics::mix(golden, h.index);
    physics::mix(golden, static_cast<uint32_t>(h.fraction.raw));
    physics::mix(golden, static_cast<uint32_t>(h.point.x.raw));
    physics::mix(golden, static_cast<uint32_t>(h.point.y.raw));
    physics::mix(golden, static_cast<uint32_t>(h.normal.x.raw));
    physics::mix(golden, static_cast<uint32_t>(h.normal.y.raw));
}

void test_overlap() {
    const TileGrid g = scene();
    TileFilter f;
    std::vector<TileOverlap> out;

    // Коробка 16x8, севшая на стык тайлов (4,6) и (5,6): центр ровно на границе x = 80.
    overlap_shape(g, physics::box(fx(8), fx(4)), {fx(80), fx(100)}, fix32{}, f, out);
    check(out.size() == 2, "a box straddling the seam overlaps exactly two floor tiles");
    if (out.size() == 2) {
        check(out[0].x == 4 && out[0].y == 6, "the left tile of the pair comes first");
        check(out[1].x == 5 && out[1].y == 6, "and the right tile second");
        check(out[0].index < out[1].index, "the answer is ordered by tile index, not by traversal");
    }
    for (const TileOverlap& o : out) physics::mix(golden, o.index);

    // Тот же зонд в воздухе: пусто. Утверждение парное к верхнему — «ничего не нашли» обязано
    // означать «нечего было найти», а не сломанный обход окна.
    overlap_shape(g, physics::box(fx(8), fx(4)), {fx(80), fx(40)}, fix32{}, f, out);
    check(out.empty(), "the same probe in open air overlaps nothing");
    physics::mix(golden, static_cast<uint32_t>(out.size()));

    // Фильтр, не совпавший ни с одним битом раскладки, обязан отдавать пусто на той же геометрии:
    // маска участия читается, а не игнорируется.
    TileFilter none{static_cast<TileFlags>(TILE_SOLID << 1)};
    overlap_shape(g, physics::box(fx(8), fx(4)), {fx(80), fx(100)}, fix32{}, none, out);
    check(out.empty(), "a filter matching no flag finds nothing on the same geometry");
}

void test_raycast() {
    const TileGrid g = scene();
    TileFilter f;
    TileHit h;

    // Сверху вниз в пол: путь 96 юнитов от y = 0, верх пола на y = 96 — доля обязана быть 1.
    const bool down = raycast(g, {fx(24), fix32{}}, {fix32{}, fx(96)}, f, h);
    check(down, "a ray dropped onto the floor hits it");
    check(h.y == 6 && h.x == 1, "in the tile the ray actually passes through");
    check(h.fraction == fix32::from_int(1), "at the very end of a path that just reaches the top");
    check(h.normal == Vec2{fix32{}, fx(-1)}, "with the normal out of the floor top");
    check(h.point.y == fx(96), "and the point on the surface, not inside it");
    feed(down, h);

    // Тот же луч короче на юнит: не долетел. Пара доказывает, что доля 1 выше — это касание, а не
    // округление «где-то там».
    TileHit miss;
    check(!raycast(g, {fx(24), fix32{}}, {fix32{}, fx(95)}, f, miss),
          "a ray one unit shorter stops short of the floor");
    feed(false, miss);

    // Вбок в колонну стены: левая грань колонны на x = 160, старт x = 24, путь 160.
    const bool side = raycast(g, {fx(24), fx(60)}, {fx(160), fix32{}}, f, h);
    check(side, "a ray fired sideways reaches the wall column");
    check(h.x == 10 && h.y == 3, "in the column tile at the ray's height");
    check(h.normal == Vec2{fx(-1), fix32{}}, "with the outward normal of its left face");
    // Точка не РОВНО на грани, и это не погрешность: продвижение останавливается на разделении не
    // больше `CONTACT_SLOP`, то есть законно не доезжает до неё на допуск. Утверждение поэтому про
    // допуск, а не про равенство, — и оно двустороннее, иначе луч, вставший на полпути, прошёл бы.
    const fix32 short_of = fx(160) - h.point.x;
    check(short_of.raw >= 0 && !(physics::CONTACT_SLOP < short_of),
          "and the point within the contact slop of that face, on the near side");
    feed(side, h);
}

void test_shapecast() {
    const TileGrid g = scene();
    TileFilter f;
    TileHit h;
    const physics::Shape body = physics::box(fx(6), fx(12));
    const Vec2 rest{fx(80), fx(96) - SKIN - fx(12)};

    // Персонаж стоит на полу с зазором SKIN и идёт вправо: впереди ступень, её левая грань на
    // x = 112, правый край корпуса на x = 86, значит остановка на (112 − 86) / 48.
    const bool step = shapecast(g, body, rest, fix32{}, {fx(48), fix32{}}, f, h);
    check(step, "a body walking right runs into the step ahead");
    check(h.x == 7 && h.y == 5, "and it is the step tile that answers");
    check(h.normal == Vec2{fx(-1), fix32{}}, "with the outward normal of its left face");
    check(h.fraction == fix32::from_int(26) / fix32::from_int(48),
          "at the fraction the layout dictates");
    feed(step, h);

    // Тот же ход влево: до одинокого тайла (2,2) корпус не достаёт по высоте, пол под ногами в
    // зазоре — путь свободен. Утверждение о ПУСТОТЕ, и оно ловит лишнюю грань.
    TileHit left;
    check(!shapecast(g, body, rest, fix32{}, {fx(-48), fix32{}}, f, left),
          "the same walk to the left is free: nothing stands in it");
    feed(false, left);

    // Падение на пол в колонке без тайлов над полом: низ корпуса на y = 48, пол на y = 96, путь
    // 96 — доля 1/2.
    const bool fall = shapecast(g, body, {fx(64), fx(36)}, fix32{}, {fix32{}, fx(96)}, f, h);
    check(fall, "a body dropped onto the floor lands on it");
    check(h.normal == Vec2{fix32{}, fx(-1)}, "with the normal out of the floor top");
    check(h.fraction == fix32::from_int(1) / fix32::from_int(2), "halfway down the path");
    feed(fall, h);
}

// Инвариант 4 спеки: цена запроса определяется ОКНОМ зонда, а не размером карты. Утверждение
// парное — счётчик обязан совпасть на картах разного размера И быть заметно меньше их площади,
// иначе «совпал» означало бы, что считать нечего.
void test_window_cost() {
    TileGrid small({fix32{}, fix32{}}, fx(16), 12, 12);
    small.fill(0, 6, 12, 8, TILE_SOLID);
    TileGrid huge({fix32{}, fix32{}}, fx(16), 200, 200);
    huge.fill(0, 6, 200, 8, TILE_SOLID);

    TileFilter f;
    TileHit h;
    const physics::Shape body = physics::box(fx(6), fx(12));
    const Vec2 at{fx(80), fx(60)};
    shapecast(small, body, at, fix32{}, {fix32{}, fx(32)}, f, h);
    const uint64_t a = small.counters().scanned;
    shapecast(huge, body, at, fix32{}, {fix32{}, fx(32)}, f, h);
    const uint64_t b = huge.counters().scanned;

    std::printf("  scanned: 12x12 = %llu, 200x200 = %llu\n", static_cast<unsigned long long>(a),
                static_cast<unsigned long long>(b));
    check(a == b, "the same probe scans the same tiles on a map 277 times larger");
    check(a > 0 && a < 12 * 12, "and scans a window, not the map");
    physics::mix(golden, static_cast<uint32_t>(a));
}

// Голден шага A. Перепечатывается только вместе с решением, изменившим ответ запроса, — и решение
// это называется в ADR, а не в сообщении коммита.
constexpr uint64_t GOLDEN = 0xb19419157787b1c3ull;

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework tilemap gate\n");
    test_overlap();
    test_raycast();
    test_shapecast();
    test_window_cost();
    std::printf("  tilemap answers hash = 0x%016llx\n", static_cast<unsigned long long>(golden));
    check(golden == GOLDEN, "the folded answers of all three queries match the golden");
    std::printf("framework-tilemap: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
