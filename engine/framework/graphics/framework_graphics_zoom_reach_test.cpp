#include <cstdio>

#include "camera.hpp"
#include "platform_args.hpp"
#include "tile_draw.hpp"
#include "viewport.hpp"

// Вертикаль 2, шаг D спеки #17: до кого зум обязан ДОЕХАТЬ. Отдельно от `..._viewport_test` по
// классу отказа: там неверное число видно числом, а здесь зум, застрявший в переводе координат,
// рисует правильную картинку в неверном окне — уровень показывает пустоту за краем, а цена кадра
// не следует за тем, сколько на самом деле видно.
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

Viewport make(fix32 zoom, int32_t ppu) {
    Viewport v;
    v.screen_half = {fix32::from_int(320), fix32::from_int(180)};
    v.zoom = zoom;
    v.pixels_per_unit = ppu;
    return v;
}

void test_zoom_reaches_the_culling_window() {
    tm::TileGrid grid(Vec2{}, fix32::from_int(16), 120, 40);
    for (uint32_t x = 0; x < 120; ++x)
        for (uint32_t y = 0; y < 40; ++y)
            if ((x + y) % 3 == 0) grid.set(x, y, tm::TILE_SOLID);

    TileSet set;
    set.region[tm::TILE_SOLID] = 7;
    const Vec2 c = {fix32::from_int(400), fix32::from_int(200)};

    SpriteList list(storage, keys, CAP);
    const Vec2 in = viewport_half_world(make(fix32::from_int(2), 4));
    const physics::Aabb near_view{c - in, c + in};
    const TileDrawStats tight = draw_tiles(list, grid, near_view, set);

    list.clear();
    const Vec2 out = viewport_half_world(make(fix32::from_float(0.5), 4));
    const physics::Aabb far_view{c - out, c + out};
    const TileDrawStats wide = draw_tiles(list, grid, far_view, set);

    std::printf("  culling: zoom 2 visits %u, zoom 1/2 visits %u\n", tight.visited, wide.visited);
    // Гейт: зум, не доехавший до culling'а, менял бы картинку, но не цену кадра — и камера у края
    // уровня показывала бы пустоту за краем. Отношение сторон вида здесь ровно вчетверо, поэтому
    // порогом стоит не «больше», а «сильно больше»: «больше на один тайл» дало бы тот же PASS.
    check(wide.visited > tight.visited * 4, "zooming out widens the culling window");
    check(tight.visited > 0 && wide.emitted > tight.emitted,
          "control: both windows really visited tiles and the wide one drew more");
}

void test_zoom_reaches_the_level_bounds() {
    CameraConfig cfg;
    cfg.policies = CAMERA_BOUNDS;
    cfg.bounds = {fix32::from_int(0), fix32::from_int(0), fix32::from_int(1000),
                  fix32::from_int(600)};
    Camera tight;
    tight.center = {fix32::from_int(5), fix32::from_int(5)};
    cfg.half_view = viewport_half_world(make(fix32::from_int(2), 4));
    camera_follow(tight, cfg, {fix32::from_int(-50), fix32::from_int(-50)}, 0);

    Camera wide;
    wide.center = {fix32::from_int(5), fix32::from_int(5)};
    cfg.half_view = viewport_half_world(make(fix32::from_int(1), 4));
    camera_follow(wide, cfg, {fix32::from_int(-50), fix32::from_int(-50)}, 0);

    // Отодвинулся зум — отодвинулась и стена: границы режут ЦЕНТР с оглядкой на половину вида, и
    // зум, не доехавший до `half_view`, показал бы за краем уровня пустоту.
    check(tight.center.x < wide.center.x && tight.center.y < wide.center.y,
          "zooming out pushes the camera further from the level edge");
    check(tight.center.x == fix32::from_int(40), "control: the tight bound really is the half view");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("camera zoom: what it must reach\n");

    test_zoom_reaches_the_culling_window();
    test_zoom_reaches_the_level_bounds();

    std::printf("framework-graphics-zoom-reach: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
