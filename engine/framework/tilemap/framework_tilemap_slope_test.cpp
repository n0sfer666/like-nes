#include <cstdio>
#include <vector>

#include "platform_args.hpp"
#include "query.hpp"

// Геометрия склона (вертикаль 3, шаг B): тайл со склоном разворачивается в ТРЕУГОЛЬНИК, а не в
// коробку, и ориентация читается из битов зеркал.
//
// Отдельной целью от `..._test`, потому что класс поломки тут свой: там окно и маска, здесь ФОРМА.
// Утверждение «склон отвечает» вакуумно само по себе — коробка отвечает на те же зонды и лучше,
// поэтому каждое здесь парное: пустая половина тайла проверяется тем же зондом, что находит
// сплошную, и он же обязан находить КОРОБКУ в той самой точке, где склон отвечает пусто.
//
// Строитель, потерявший биты зеркал, красит три ориентации из четырёх: у трёх пустая половина
// лежит там, где у `slope_br` сплошное или сама гипотенуза.
namespace {

int fails = 0;

void check(bool ok, const char* what, const char* who = "") {
    if (!ok) {
        std::printf("  FAIL: %s%s%s\n", who, *who != 0 ? ": " : "", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::tilemap;

fix32 fx(int v) { return fix32::from_int(v); }

constexpr uint32_t TX = 3;
constexpr uint32_t TY = 3;

TileGrid one_tile(TileFlags flags) {
    TileGrid g({fix32{}, fix32{}}, fx(16), 8, 8);
    g.set(TX, TY, flags);
    return g;
}

Vec2 centre() { return {fx(56), fx(56)}; }

// Смещения названы от центра тайла и зеркальны друг другу: пустая и сплошная половины склона — это
// одна пара точек, отражённая через центр, и записывать их порознь значило бы дать опечатке
// возможность совпасть с ошибкой строителя.
struct Orient {
    const char* name;
    TileFlags flags;
    Vec2 hollow;
};

const Orient ORIENTS[] = {
    {"slope_br", TILE_SOLID | TILE_SLOPE, {fx(-4), fx(-4)}},
    {"slope_bl", TILE_SOLID | TILE_SLOPE | TILE_SLOPE_FLIP_X, {fx(4), fx(-4)}},
    {"slope_tr", TILE_SOLID | TILE_SLOPE | TILE_SLOPE_FLIP_Y, {fx(-4), fx(4)}},
    {"slope_tl", TILE_SOLID | TILE_SLOPE | TILE_SLOPE_FLIP_X | TILE_SLOPE_FLIP_Y, {fx(4), fx(4)}},
};

void test_halves() {
    const physics::Shape probe = physics::box(fx(2), fx(2));
    const TileGrid box_tile = one_tile(TILE_SOLID);
    TileFilter f;
    std::vector<TileOverlap> out;
    for (const Orient& o : ORIENTS) {
        const TileGrid g = one_tile(o.flags);
        const Vec2 hollow = centre() + o.hollow;
        const Vec2 solid{centre().x - o.hollow.x, centre().y - o.hollow.y};

        overlap_shape(g, probe, hollow, fix32{}, f, out);
        check(out.empty(), "the hollow half of the tile answers empty", o.name);

        overlap_shape(g, probe, solid, fix32{}, f, out);
        check(out.size() == 1 && out[0].x == static_cast<int32_t>(TX) &&
                  out[0].y == static_cast<int32_t>(TY),
              "the solid half answers with the tile", o.name);

        // Парный контроль зонда: в том же месте коробка обязана отвечать. Без него «пусто» выше
        // проходило бы и у запроса, потерявшего тайл целиком.
        overlap_shape(box_tile, probe, hollow, fix32{}, f, out);
        check(out.size() == 1, "a plain solid tile answers at that same point", o.name);
    }
}

// Падение на гипотенузу. Нормаль здесь не «примерно наклонная», а РОВНО 45°: на этом равенстве
// стоит порог проходимости шага B1b, и округление, съевшее единицу младшего разряда, сдвинуло бы
// границу «пол или стена» на тайл, который выглядит одинаково.
void test_fall_onto_the_hypotenuse() {
    const TileGrid slope = one_tile(TILE_SOLID | TILE_SLOPE);
    const TileGrid box_tile = one_tile(TILE_SOLID);
    const physics::Shape body = physics::box(fx(4), fx(4));
    const Vec2 from{fx(56), fx(40)};
    const Vec2 travel{fix32{}, fx(32)};
    TileFilter f;

    TileHit h;
    check(shapecast(slope, body, from, fix32{}, travel, f, h), "a fall onto a slope tile lands");
    check(h.normal.y < fix32{}, "the hypotenuse answers with a normal pointing up");
    check(h.normal.x == h.normal.y, "and tilted by exactly 45 degrees: n.x == n.y");

    TileHit b;
    check(shapecast(box_tile, body, from, fix32{}, travel, f, b), "the same fall onto a box lands");
    check(b.normal == Vec2{fix32{}, fx(-1)}, "on a box tile the normal is axial");
    check(b.fraction < h.fraction, "and the slope stops the body lower than the box would");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tilemap slope geometry\n");
    test_halves();
    test_fall_onto_the_hypotenuse();
    std::printf("framework-tilemap-slope: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
