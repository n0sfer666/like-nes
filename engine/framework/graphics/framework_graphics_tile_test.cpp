#include <cstdio>

#include "platform_args.hpp"
#include "tile_draw.hpp"

// Culling тайловой карты (спека #17, вертикаль 2, шаг B). Гейт 7 спрашивает не «видно ли тайлы», а
// «зависит ли цена кадра от размера уровня», и утверждается это ЧИСЛОМ обойдённых тайлов на двух
// картах разного размера под одним видом. Число спрайтов на этот вопрос не отвечает: пустая карта
// даёт ноль при любом окне.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::graphics;
namespace tm = framework::tilemap;

constexpr uint32_t CAP = 4096;
Sprite storage[CAP];
uint64_t keys[CAP];
Batch batches[CAP];

constexpr RegionId R_SOLID = 3;
constexpr RegionId R_ONEWAY = 7;
constexpr RegionId R_LADDER = 11;

TileSet make_set() {
    TileSet set;
    set.region[tm::TILE_SOLID] = R_SOLID;
    set.region[tm::TILE_SOLID | tm::TILE_ONEWAY] = R_ONEWAY;
    set.region[tm::TILE_LADDER] = R_LADDER;
    set.rgba = 0x40a0ffffu;
    set.material = 2;
    set.layer = -1;
    return set;
}

// Одна и та же раскладка на карте любого размера: пол по нижнему краю, лестница столбом и полка из
// односторонних тайлов. Содержимое ВИДА поэтому от размера карты не зависит, и разойдись число
// спрайтов — это culling, а не другая карта.
tm::TileGrid make_map(uint32_t w, uint32_t h) {
    tm::TileGrid g({fix32::from_int(-64), fix32::from_int(-32)}, fix32::from_int(16), w, h);
    for (uint32_t x = 0; x < w; ++x) g.set(x, h - 1, tm::TILE_SOLID);
    for (uint32_t y = 0; y < h; ++y) g.set(3, y, tm::TILE_LADDER);
    for (uint32_t x = 5; x < 12 && x < w; ++x) g.set(x, h / 2, tm::TILE_SOLID | tm::TILE_ONEWAY);
    return g;
}

physics::Aabb view_at(fix32 left, fix32 top) {
    return physics::Aabb{{left, top}, {left + fix32::from_int(320), top + fix32::from_int(240)}};
}

// Гейт 7 спеки #17: карта растёт в тридцать раз — цена кадра не меняется.
void test_cost_does_not_follow_map_size() {
    const TileSet set = make_set();
    SpriteList list(storage, keys, CAP);

    tm::TileGrid small = make_map(40, 30);
    const physics::Aabb view = view_at(fix32::from_int(-64), fix32::from_int(-32));
    const TileDrawStats a = draw_tiles(list, small, view, set);

    // Тридцатикратный уровень: та же высота и раскладка, ширина в тридцать раз.
    list.clear();
    tm::TileGrid big = make_map(1200, 30);
    const TileDrawStats b = draw_tiles(list, big, view, set);

    std::printf("  visited: %ux%u map = %u, %ux%u map = %u\n", small.width(), small.height(),
                a.visited, big.width(), big.height(), b.visited);
    check(a.visited == b.visited, "the window is the cost, not the map");
    check(a.emitted == b.emitted, "the same view over the same layout draws the same sprites");
    // Контроля два, и без них равенство выше зелено вакуумно: обход, не нашедший ничего, совпал бы
    // сам с собой на любых картах, а карта размером с окно не дала бы чему расти.
    check(a.visited > 0 && a.emitted > 0, "control: the view really does cover part of the map");
    check(b.visited * 20 < big.width() * big.height(), "control: the big map dwarfs the window");
}

void test_window_is_the_view_not_the_map() {
    const TileSet set = make_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = make_map(200, 40);
    const fix32 x0 = fix32::from_int(-64);
    const fix32 y0 = fix32::from_int(-32);
    // Вид, кончающийся ВНУТРИ тайла: 319x239 юнитов при тайле 16 — ровно те тайлы, которые он
    // накрывает, и ни одним больше.
    const physics::Aabb inside{{x0, y0}, {x0 + fix32::from_int(319), y0 + fix32::from_int(239)}};
    check(draw_tiles(list, g, inside, set).visited == 20 * 15, "the window is the view, not the map");
    // Тот же вид, сдвинутый на полтайла: он задевает лишний столбец, и тот обязан рисоваться.
    list.clear();
    const physics::Aabb shifted{{x0 + fix32::from_int(8), y0},
                                {x0 + fix32::from_int(327), y0 + fix32::from_int(239)}};
    check(draw_tiles(list, g, shifted, set).visited == 21 * 15,
          "a view off the grid draws the column it partly covers");
    // Вид, кончающийся РОВНО на границе тайла, эту границу включает. Окно здесь заимствовано у
    // сетки коллизии (`TileGrid::window`), где включение обязательно: зонд впритык к стене обязан
    // стену видеть. Для отрисовки это стоит одного лишнего ряда квадов за краем экрана — и это
    // дешевле второго контракта окна, расходящегося с первым на округлении.
    list.clear();
    check(draw_tiles(list, g, view_at(x0, y0), set).visited == 21 * 16,
          "a view ending on a tile boundary draws that tile: the window is the collision window");
}

void test_geometry_and_regions() {
    const TileSet set = make_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = make_map(40, 30);
    draw_tiles(list, g, view_at(fix32::from_int(-64), fix32::from_int(-32)), set);

    // Первый поданный спрайт — лестница в столбце 3 строки 0: обход построчный сверху вниз.
    const Sprite& first = list.data()[0];
    const physics::Aabb box = g.tile_bounds(3, 0);
    check(first.region == R_LADDER, "a tile draws the region its own bits name");
    check(first.center.x.raw == ((box.min.x + box.max.x).raw / 2) &&
              first.center.y.raw == ((box.min.y + box.max.y).raw / 2),
          "a tile sprite sits at the centre of its own tile box");
    check(first.half.x.raw == ((box.max.x - box.min.x).raw / 2), "a tile sprite is as wide as its tile");
    check(first.material == 2 && first.layer == -1 && first.rgba == 0x40a0ffffu,
          "the tileset owns material, layer and tint");

    // Пустой тайл региона не получает вовсе — иначе прозрачный квад занимал бы слот батча.
    uint32_t solid = 0, oneway = 0, ladder = 0;
    for (uint32_t i = 0; i < list.count(); ++i) {
        const RegionId r = list.data()[i].region;
        solid += r == R_SOLID;
        oneway += r == R_ONEWAY;
        ladder += r == R_LADDER;
    }
    check(solid + oneway + ladder == list.count(), "no tile draws a region the tileset did not name");
    check(oneway == 7, "a one-way tile draws the one-way region, not the solid one");
    check(ladder == 16, "the ladder column is drawn from top to bottom");
}

void test_one_material_is_one_draw_call() {
    const TileSet set = make_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = make_map(200, 40);
    const TileDrawStats a = draw_tiles(list, g, view_at(fix32::from_int(-64), fix32::from_int(-32)), set);
    // Вся карта на одном материале — значит один вызов отрисовки, сколько бы тайлов ни попало в
    // окно. Это шаг A на СВОЁМ потребителе: тайлы и есть сцена, ради которой батчинг заводился.
    check(list.build(batches, CAP) == 1, "a tilemap on one material costs one draw call");
    check(batches[0].count == a.emitted, "every emitted tile is inside that one draw call");
}

uint64_t fold(const SpriteList& list) {
    uint64_t h = 0xcbf29ce484222325ull;
    const auto mix = [&h](uint64_t v) { h = (h ^ v) * 0x100000001b3ull; };
    for (uint32_t i = 0; i < list.count(); ++i) {
        const Sprite& s = list.drawn(i);
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.center.x.raw)));
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.center.y.raw)));
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.half.x.raw)));
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.half.y.raw)));
        mix(s.region);
    }
    return h;
}

constexpr uint64_t GOLDEN = 0x599044fa7aabfddcull;

void test_golden() {
    const TileSet set = make_set();
    SpriteList list(storage, keys, CAP);
    // Карта высотой 32 выбрана не для краткости: полка односторонних тайлов лежит на середине, и
    // только на такой высоте вид захватывает разом все ТРИ региона — пол, полку и лестницу. Голден
    // одного региона пинил бы одну ветку таблицы из трёх.
    tm::TileGrid g = make_map(200, 32);
    // Вид НЕ выровнен по сетке и стоит не в начале карты: выровненный не задел бы ни округление
    // окна вниз, ни отрицательные мировые координаты — то есть ровно то, на чём деление усекается
    // к нулю вместо пола.
    const TileDrawStats st = draw_tiles(list, g, view_at(fix32::from_int(-53), fix32::from_int(229)), set);
    list.build(batches, CAP);
    const uint64_t h = fold(list);
    std::printf("  tiles: visited %u, emitted %u, unknown %u\n", st.visited, st.emitted, st.unknown);
    std::printf("  tile-draw hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == GOLDEN, "the tile sprite stream matches the golden");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tilemap culling and tile sprites\n");

    test_cost_does_not_follow_map_size();
    test_window_is_the_view_not_the_map();
    test_geometry_and_regions();
    test_one_material_is_one_draw_call();
    test_golden();

    std::printf("framework-graphics-tile: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
