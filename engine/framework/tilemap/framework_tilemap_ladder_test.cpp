#include <cstdio>
#include <vector>

#include "platform_args.hpp"
#include "query.hpp"

// Лестница (вертикаль 3, шаг D): бит карты, который НЕ геометрия.
//
// Своей целью от `..._oneway_test` по тому же основанию, по которому та отделена от склона: там
// предмет — правило отбора хита у тайла, который держит, а здесь — тайл, который не держит ВОВСЕ и
// виден только запросу, спросившему его прямо. Классы поломки разные, и различать их в логе CI
// обязано имя цели.
//
// Каждое утверждение парное, и пара тут несущая ровно потому, что половина утверждений — про
// ОТСУТСТВИЕ ответа: «свип не нашёл лестницу» — правда и про пустую сетку, и про промах зонда, и
// про сломанное окно. Пара расходится с ним одним битом карты, и один из двух ответов обязан быть.
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

// Тайл (3,3) занимает x [48,64], y [48,64]: те же координаты, что у гейта односторонней платформы,
// чтобы числа зондов читались рядом.
constexpr uint32_t TX = 3;
constexpr uint32_t TY = 3;

constexpr TileFlags LADDER = TILE_LADDER;
constexpr TileFlags LANDING = TILE_SOLID | TILE_ONEWAY | TILE_LADDER;

TileGrid one_tile(TileFlags flags) {
    TileGrid g({fix32{}, fix32{}}, fx(16), 8, 8);
    g.set(TX, TY, flags);
    return g;
}

physics::Shape mover() { return physics::sanitize(physics::box(fx(6), fx(6))); }

// Свип сверху вниз сквозь тайл: низ формы на старте 42, то есть выше верха тайла (48). Ровно тот
// путь, которым персонаж падает на пол, — и лестница обязана его пропустить.
bool falls_through(const TileGrid& g) {
    TileHit hit;
    return !shapecast(g, mover(), {fx(56), fx(36)}, fix32{}, {fix32{}, fx(20)}, TileFilter{}, hit);
}

// Кто здесь по фильтру: форма стоит В ЦЕНТРЕ тайла (56,56).
std::size_t here(const TileGrid& g, TileFlags require) {
    TileFilter f;
    f.require = require;
    std::vector<TileOverlap> out;
    overlap_shape(g, mover(), {fx(56), fx(56)}, fix32{}, f, out);
    return out.size();
}

// Тело: лестница им не является, и спрашивается это тем же путём, которым геометрию спрашивает
// контроллер.
void test_body() {
    const TileGrid ladder = one_tile(LADDER);
    const TileGrid solid = one_tile(TILE_SOLID);

    check(falls_through(ladder), "a sweep falls straight through a ladder tile");
    check(!falls_through(solid), "the same sweep is stopped by a solid tile");

    check(!ladder.solid_at(TX, TY), "a ladder tile is not solid");
    check(solid.solid_at(TX, TY), "the paired tile is");

    check(here(ladder, TILE_SOLID) == 0, "a solid query does not see the ladder");
    check(here(solid, TILE_SOLID) == 1, "the same query sees the paired tile");
}

// Метка: лестница видна ТОЛЬКО запросу, спросившему её прямо. Пара тут не косметическая — фильтр,
// потерявший `require`, ответил бы «нашёл» на обеих сетках, и первая половина осталась бы зелёной.
void test_mark() {
    const TileGrid ladder = one_tile(LADDER);
    const TileGrid solid = one_tile(TILE_SOLID);

    check(here(ladder, TILE_LADDER) == 1, "a ladder query finds the ladder tile");
    check(here(solid, TILE_LADDER) == 0, "the same query finds nothing on a solid tile");
}

// Верхняя площадка `solid oneway ladder` — единственная законная пара лестницы с телом (отказ на
// прочих проверяет `..._refusal_test`). Утверждение двойное и ровно про то, что биты не мешают друг
// другу: правило односторонности не теряется от метки, а метка не теряется от тела.
void test_landing() {
    const TileGrid landing = one_tile(LANDING);
    const TileGrid ladder = one_tile(LADDER);

    check(!falls_through(landing), "the landing still holds someone coming from above");
    check(falls_through(ladder), "the ladder below it still does not");

    check(here(landing, TILE_LADDER) == 1, "the landing is still a ladder to a ladder query");
    check(here(landing, TILE_SOLID) == 1, "and still a body to a solid one");

    TileHit hit;
    check(!shapecast(landing, mover(), {fx(56), fx(80)}, fix32{}, {fix32{}, fx(-20)}, TileFilter{},
                     hit),
          "and is still climbed into from below");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tilemap ladder tile\n");
    test_body();
    test_mark();
    test_landing();
    std::printf("framework-tilemap-ladder: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
