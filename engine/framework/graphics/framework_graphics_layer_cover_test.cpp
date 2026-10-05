#include <cstdio>
#include <string>

#include "layer_cover_fixture.hpp"
#include "platform_args.hpp"

// Покрытие слоёв на бейке (спека #24, В7). Сломанные фикстуры — в обе стороны: законный арт
// (дальний фон с повтором, небо `p = 0`) обязан проходить, дыра — отбиваться с именем слоя и
// числом пикселей. Отказ судится по ТЕКСТУ причины: «отбит» без неё засчитал бы отказ по чужой
// причине — скажем, слой отбит правилом повтора, а геометрия полосы не проверена вовсе.
//
// Карта 40×21 тайл по 16 — 640×336. Предел 576×312, зона 384×216 и потолок тряски 8: слою с
// `p = 1` хватает карты ровно при bounds 104..536 × 56..280, поэтому каждый край — на пикселе.
namespace {

using namespace layer_cover_fixture;

void test_bounds_edges() {
    const VisualMapSrc m = map_of({tile_layer("street")});
    passes(m, FIT, "a map-sized layer at p = 1 covers bounds inset by the limit margin");
    refuses(m, ObjectMapSrc{},
            gap("street", "x short by 104 px left, 104 px right; y short by 56 px top, 56 px bottom"),
            "without a bounds object the map is the bounds, and its own edges show");
    refuses(m, bounds(103, 56, 536, 280), gap("street", "x short by 1 px left"),
            "bounds one pixel left of the margin");
    refuses(m, bounds(104, 56, 537, 280), gap("street", "x short by 1 px right"),
            "bounds one pixel right of the margin");
    refuses(m, bounds(104, 55, 536, 280), gap("street", "y short by 1 px top"),
            "bounds one pixel above the margin");
    refuses(m, bounds(104, 56, 536, 281), gap("street", "y short by 1 px bottom"),
            "bounds one pixel below the margin");
    passes(m, bounds(296, 128, 344, 208),
           "bounds narrower than the zone park the camera at their middle, not at a swapped edge");
    refuses(m, bounds(0, 56, 48, 280), gap("street", "x short by 272 px left"),
            "narrow bounds at the map edge are judged from their middle, not edge plus half");
}

void test_offset_sign() {
    VisualLayerSrc right = tile_layer("street");
    right.offset_x = fix32::from_int(16);
    refuses(map_of({right}), FIT, gap("street", "x short by 16 px left"),
            "an offset to the right uncovers the left edge");
    VisualLayerSrc up = tile_layer("street");
    up.offset_y = fix32::from_int(-16);
    refuses(map_of({up}), FIT, gap("street", "y short by 16 px bottom"),
            "an offset upwards uncovers the bottom edge");

    // Привязка к пикселю округляет центр слоя ВНИЗ до целого: при дробном смещении порог полосы
    // нецелый, и центр 288.5 (bounds 104.5) рисуется из 288 — полпикселя смещения уже дыра.
    VisualLayerSrc half = tile_layer("street");
    half.offset_x = fix32::from_float(0.5);
    ObjectMapSrc snapped = FIT;
    snapped.objects[1].x = fix32::from_float(104.5);
    refuses(map_of({half}), snapped, gap("street", "x short by 1 px left"),
            "a fractional offset meets a centre the pixel snap pulls down");
    passes(map_of({half}), bounds(105, 56, 536, 280), "the same offset one pixel further in");
}

void test_parallax() {
    VisualLayerSrc far = tile_layer("far");
    far.parallax_x = far.parallax_y = fix32::from_float(0.5);
    far.repeat = tilemap::REPEAT_X;
    refuses(map_of({far}), FIT, gap("far", "y short by 78 px top"),
            "a map-sized layer at p = 0.5 lags the camera and shows its top edge");

    VisualLayerSrc bare = image_layer("haze", 128, 336);
    bare.parallax_x = bare.parallax_y = fix32::from_float(0.3);
    refuses(map_of({bare}), FIT,
            "map 'lv': layer 'haze' has parallax x below 1 and must repeat on x",
            "a background at p = 0.3 without repeat");

    VisualLayerSrc haze = bare;
    haze.repeat = tilemap::REPEAT_X;
    haze.offset_y = fix32::from_int(-112);
    passes(map_of({haze}), FIT, "the same background repeating on x and placed for its band");

    VisualLayerSrc sky = image_layer("sky", 64, 312);
    sky.parallax_x = sky.parallax_y = fix32{};
    sky.repeat = tilemap::REPEAT_X;
    sky.offset_y = fix32::from_int(-156);
    passes(map_of({sky}), FIT, "a sky at p = 0 with repeat stands on the screen");
    sky.image_h = 311;
    refuses(map_of({sky}), FIT, gap("sky", "y short by 1 px bottom"), "a sky a pixel short");

    VisualLayerSrc front = tile_layer("front");
    front.parallax_x = fix32::from_float(1.25);
    refuses(map_of({front}), FIT, gap("front", "x short by 88 px right"),
            "a foreground at p = 1.25 outruns the camera and shows its right edge");

    VisualLayerSrc tiled = tile_layer("street");
    tiled.repeat = tilemap::REPEAT_X | tilemap::REPEAT_Y;
    passes(map_of({tiled}), ObjectMapSrc{}, "a layer repeating on both axes covers any view");
}

void test_cover_y() {
    VisualLayerSrc city = image_layer("city", 144, 124);
    city.parallax_x = city.parallax_y = fix32::from_float(0.5);
    city.repeat = tilemap::REPEAT_X;
    refuses(map_of({city}), FIT, gap("city", "y short by 78 px top, 122 px bottom"),
            "a skyline silhouette is judged on y unless it declares itself decor");
    city.cover_y = false;
    passes(map_of({city}), FIT, "cover_y = false lifts the y axis");
    city.repeat = 0;
    refuses(map_of({city}), FIT, "map 'lv': layer 'city' has parallax x below 1 and must repeat on x",
            "cover_y = false keeps the repeat rule");

    VisualLayerSrc street = tile_layer("street");
    street.cover_y = false;
    passes(map_of({street}), bounds(104, 0, 536, 336), "a y-decor layer ignores bounds past its height");
    refuses(map_of({street}), bounds(103, 0, 536, 336), gap("street", "x short by 1 px left"),
            "cover_y = false keeps the x axis");
}

void test_refusals() {
    refuses(map_of({tile_layer("street"), tile_layer("hole")}), bounds(103, 56, 536, 280),
            gap("street", "x short by 1 px left"), "the first layer that fails is the one named");
    ObjectMapSrc two = FIT;
    two.objects.push_back(rect("bounds", 0, 0, 640, 336));
    refuses(map_of({tile_layer("street")}), two, "map 'lv' has two objects of class bounds",
            "two bounds objects are ambiguous");
    ObjectMapSrc point = FIT;
    point.objects[1].shape = ObjectShape::Point;
    refuses(map_of({tile_layer("street")}), point,
            "map 'lv': bounds object 'street' must be a rectangle", "a bounds point is refused");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("layer cover: every layer covers what the viewport limit shows\n");
    test_bounds_edges();
    test_offset_sign();
    test_parallax();
    test_cover_y();
    test_refusals();

    std::printf("framework-graphics-layer-cover: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
