#include <cstdio>

#include "platform_args.hpp"
#include "tile_draw.hpp"

// Отказы шва «тайлы → спрайты» (спека #17, вертикаль 2, шаг B). Отдельная цель по тому же
// основанию, что у шага A: «нарисовал не то» видно по координате, а «молча потерял» — только по
// счётчику, и в одном файле второе тонет.
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

constexpr uint32_t CAP = 512;
Sprite storage[CAP];
uint64_t keys[CAP];

TileSet full_set() {
    TileSet set;
    for (uint32_t i = 1; i < TILE_KINDS; ++i) set.region[i] = static_cast<RegionId>(i);
    return set;
}

tm::TileGrid solid_map(uint32_t w, uint32_t h) {
    tm::TileGrid g({fix32{}, fix32{}}, fix32::from_int(16), w, h);
    g.fill(0, 0, w, h, tm::TILE_SOLID);
    return g;
}

physics::Aabb box(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    return physics::Aabb{{fix32::from_int(x0), fix32::from_int(y0)},
                         {fix32::from_int(x1), fix32::from_int(y1)}};
}

void test_view_off_map() {
    const TileSet set = full_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = solid_map(10, 10);
    // Карта занимает 0..160 по обеим осям. Вид целиком левее и целиком правее — ноль обойдённых
    // тайлов и НОЛЬ ПОТЕРЬ: пустой вид это не переполнение.
    const TileDrawStats a = draw_tiles(list, g, box(-500, -500, -400, -400), set);
    const TileDrawStats b = draw_tiles(list, g, box(400, 400, 500, 500), set);
    check(a.visited == 0 && b.visited == 0, "a view outside the map visits nothing");
    check(a.emitted == 0 && b.emitted == 0, "a view outside the map draws nothing");
    check(list.count() == 0 && list.dropped() == 0, "an empty view is not a loss");
}

void test_view_straddles_the_edge() {
    const TileSet set = full_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = solid_map(10, 10);
    // Вид наполовину за картой. Окно обязано быть отсечено по карте, а не читать вне массива, —
    // и обойдено ровно то, что от карты осталось внутри вида (тайлы 0..5 по обеим осям: правый
    // край окна включающий).
    const TileDrawStats a = draw_tiles(list, g, box(-80, -80, 80, 80), set);
    check(a.visited == 6 * 6, "the window is clipped to the map, not to the view");
    check(a.emitted == a.visited, "every clipped tile is a real tile");
    // И симметрично на дальнем краю: карта кончается на 160, вид идёт до 240 — остаются тайлы 5..9.
    list.clear();
    const TileDrawStats b = draw_tiles(list, g, box(80, 80, 240, 240), set);
    check(b.visited == 5 * 5, "the far edge clips against the map too");
}

void test_view_covers_the_whole_map() {
    const TileSet set = full_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = solid_map(10, 10);
    const TileDrawStats a = draw_tiles(list, g, box(-1000, -1000, 1000, 1000), set);
    // Позитивный контроль отсечения: вид, накрывший карту целиком, обходит ВСЮ карту и ни тайлом
    // больше. Без него «окно отсечено» было бы правдой и у обхода, не находящего ничего.
    check(a.visited == 100, "a view over the whole map visits the whole map, and no more");
}

void test_degenerate_view() {
    const TileSet set = full_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = solid_map(10, 10);
    // Вид нулевой площади вырожден не до пустоты: точка лежит ВНУТРИ тайла, и окно отдаёт тот
    // тайл — то же включение края, которым зонд впритык к стене её находит.
    const TileDrawStats a = draw_tiles(list, g, box(48, 48, 48, 48), set);
    check(a.visited == 1 && a.emitted == 1, "a view of zero size is still inside one tile");
    // А вот вид, вывернутый наизнанку (max < min), обязан дать ПУСТОЕ окно, а не отрицательный
    // размах в цикле.
    list.clear();
    const TileDrawStats b = draw_tiles(list, g, box(100, 100, 20, 20), set);
    check(b.visited == 0 && b.emitted == 0, "an inside-out view draws nothing");
    check(list.count() == 0 && list.dropped() == 0, "an inside-out view is not a loss");
}

void test_overflow_is_counted() {
    const TileSet set = full_set();
    // Список меньше вида: 7x7 тайлов при ёмкости 20. Потеря обязана быть СОСЧИТАНА, а не выглядеть
    // как кадр, который так и задумывали.
    Sprite small_storage[20];
    uint64_t small_keys[20];
    SpriteList list(small_storage, small_keys, 20);
    tm::TileGrid g = solid_map(10, 10);
    const TileDrawStats a = draw_tiles(list, g, box(0, 0, 96, 96), set);
    check(a.visited == 49, "the walk does not stop at the list's edge");
    check(a.emitted == 49, "the walk reports what it handed over");
    check(list.count() == 20 && list.dropped() == 29, "the list counts what it could not take");
}

void test_tileset_of_zeros() {
    TileSet set;   // все регионы нулевые
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = solid_map(10, 10);
    const TileDrawStats a = draw_tiles(list, g, box(0, 0, 160, 160), set);
    // Тот самый контроль, ради которого `visited` и `emitted` считаются раздельно: обход шёл, а
    // рисовать было нечем. Слейся они в одно число — «culling работает» и «тайлсет пуст» стали бы
    // неразличимы.
    check(a.visited == 100, "a tileset of zeros does not stop the walk");
    check(a.emitted == 0 && list.count() == 0, "region zero means do not draw");
    check(list.dropped() == 0, "a tile nobody asked to draw is not a loss");
}

void test_empty_tile_draws_nothing() {
    const TileSet set = full_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g({fix32{}, fix32{}}, fix32::from_int(16), 10, 10);
    g.set(2, 2, tm::TILE_SOLID);
    const TileDrawStats a = draw_tiles(list, g, box(0, 0, 160, 160), set);
    // `full_set()` не именует региона для пустоты намеренно: индекс 0 таблицы обязан остаться
    // нулевым, иначе пустой тайл рисовал бы прозрачный квад и занимал слот батча.
    check(a.visited == 100 && a.emitted == 1, "an empty tile draws nothing");
    check(list.count() == 1 && list.data()[0].region == tm::TILE_SOLID, "the one solid tile is drawn");
}

void test_flags_outside_the_table() {
    const TileSet set = full_set();
    SpriteList list(storage, keys, CAP);
    tm::TileGrid g = solid_map(4, 4);
    // Бит выше тех, что знает таблица. Такой тайл — НЕ ошибка карты и не повод падать: биты
    // заводятся вместе с механикой, и карта, испечённая новее движка, обязана рисоваться остальным
    // содержимым. Но и молчать нельзя — иначе «тайлы пропали» неотличимо от «их там не было».
    g.set(1, 1, static_cast<tm::TileFlags>(1u << 9));
    // И тайл РОВНО на границе таблицы: `TILE_KINDS` — размер, а не последний индекс, и `>` вместо
    // `>=` читало бы здесь за концом массива. Голден из середины диапазона к этому слеп.
    g.set(2, 2, static_cast<tm::TileFlags>(TILE_KINDS));
    const TileDrawStats a = draw_tiles(list, g, box(0, 0, 64, 64), set);
    check(a.visited == 16, "a tile the tileset cannot name does not stop the walk");
    check(a.unknown == 2, "a tile outside the table is counted, not swallowed");
    check(a.emitted == 14, "the rest of the map is drawn as usual");
}

void test_no_storage() {
    const TileSet set = full_set();
    SpriteList list(nullptr, nullptr, 4096);
    tm::TileGrid g = solid_map(10, 10);
    const TileDrawStats a = draw_tiles(list, g, box(0, 0, 160, 160), set);
    // Список без буферов принимает ноль спрайтов (шаг A), и шов обязан пережить это счётчиком, а
    // не разыменованием нуля.
    check(a.visited == 100 && a.emitted == 100, "the walk itself needs no buffer");
    check(list.count() == 0 && list.dropped() == 100, "a list without storage loses everything, loudly");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("tile draw refusals\n");

    test_view_off_map();
    test_view_straddles_the_edge();
    test_view_covers_the_whole_map();
    test_degenerate_view();
    test_overflow_is_counted();
    test_tileset_of_zeros();
    test_empty_tile_draws_nothing();
    test_flags_outside_the_table();
    test_no_storage();

    std::printf("framework-graphics-tile-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
