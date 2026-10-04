#include <cstdio>
#include <string>

#include "layer_cover.hpp"
#include "platform_args.hpp"

// Покрытие слоёв на бейке (спека #24, В7). Сломанные фикстуры — в обе стороны: законный арт
// (дальний фон с повтором, небо `p = 0`) обязан проходить, дыра — отбиваться с именем слоя и
// числом пикселей. Отказ судится по ТЕКСТУ причины: «отбит» без неё засчитал бы отказ по чужой
// причине — скажем, слой отбит правилом повтора, а геометрия полосы не проверена вовсе.
//
// Карта 40×21 тайл по 16 — 640×336. Предел 576×312, зона 384×216 и потолок тряски 8: слою с
// `p = 1` хватает карты ровно при bounds 104..536 × 56..280, поэтому каждый край — на пикселе.
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

VisualLayerSrc tile_layer(const char* name) {
    VisualLayerSrc l;
    l.name = name;
    return l;
}

VisualLayerSrc image_layer(const char* name, uint32_t w, uint32_t h) {
    VisualLayerSrc l = tile_layer(name);
    l.kind = LayerKind::Image;
    l.image_w = w;
    l.image_h = h;
    return l;
}

VisualMapSrc map_of(std::initializer_list<VisualLayerSrc> layers) {
    VisualMapSrc m;
    m.name = "lv";
    m.width = 40;
    m.height = 21;
    m.tile_size = 16;
    m.layers = layers;
    return m;
}

ObjectSrc rect(const char* cls, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    ObjectSrc o;
    o.name = "street";
    o.cls = cls;
    o.shape = ObjectShape::Rect;
    o.x = fix32::from_int(x0);
    o.y = fix32::from_int(y0);
    o.w = fix32::from_int(x1 - x0);
    o.h = fix32::from_int(y1 - y0);
    return o;
}

ObjectMapSrc bounds(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    ObjectMapSrc m;
    m.name = "lv";
    m.objects.push_back(rect("spawn", 0, 0, 1, 1));
    m.objects.push_back(rect("bounds", x0, y0, x1, y1));
    return m;
}

const ObjectMapSrc FIT = bounds(104, 56, 536, 280);

void passes(const VisualMapSrc& m, const ObjectMapSrc& o, const char* what) {
    std::string error;
    const bool ok = framework::graphics::check_layer_cover(m, o, error);
    if (!ok) std::printf("  refused: %s\n", error.c_str());
    check(ok, what);
}

std::string gap(const char* layer, const char* what) {
    return std::string("map 'lv': layer '") + layer + "' does not cover the view: " + what;
}

void refuses(const VisualMapSrc& m, const ObjectMapSrc& o, const std::string& reason,
             const char* what) {
    std::string error;
    const bool ok = framework::graphics::check_layer_cover(m, o, error);
    const bool right = !ok && error == reason;
    if (!right)
        std::printf("  got %s: %s\n  want: %s\n", ok ? "pass" : "refusal", error.c_str(),
                    reason.c_str());
    check(right, what);
}

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
    test_refusals();

    std::printf("framework-graphics-layer-cover: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
