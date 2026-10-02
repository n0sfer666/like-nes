#include <cstdio>

#include "layer_fixture.hpp"
#include "platform_args.hpp"

namespace {

using namespace framework::graphics;
using namespace framework::tilemap;
using namespace layer_fixture;

void test_geometry() {
    Fixture f;
    VisualLayerSrc l = tile_layer(4, 2);
    l.cells = {1, static_cast<uint16_t>(2 | CELL_FLIP_H | CELL_FLIP_D), 0, 4, 9, 10, 11, 12};
    l.opacity = 128;
    bake(f, 4, 2, {l});
    SpriteList list(storage, keys, CAP);
    const LayerDrawStats s = run(list, f, frame_at(64, 32));
    check(s.visited == 8 && s.emitted == 7 && s.unknown == 0,
          "4x2 layer visits 8 cells, empty one skipped");
    const Sprite& a = list.data()[0];
    check(a.center == Vec2{fx(8), fx(8)} && a.half == Vec2{fx(8), fx(8)},
          "first cell sits at the screen corner");
    check(a.rgba == 0xffffff80u && a.layer == 3, "opacity and draw order reach the sprite");
    const Sprite& b = list.data()[1];
    check(b.flip == (SPRITE_FLIP_H | SPRITE_FLIP_D), "cell flip bits reach the sprite");
    check(list.data()[3].material == 1 && list.data()[3].region == 9, "second tileset is the second texture");
    check(list.build(batches, CAP) == 2, "two textures on one layer are two batches");
}

void test_parallax_and_offset() {
    Fixture f;
    VisualLayerSrc far = tile_layer(4, 2);
    far.parallax_x = half_of(1);
    far.offset_y = fx(4);
    bake(f, 4, 2, {far, tile_layer(4, 2)});
    SpriteList list(storage, keys, CAP);
    run(list, f, frame_at(100, 32), 0);
    check(list.data()[0].center == Vec2{fx(14 + 8), fx(4 + 8)}, "half parallax layer moves half as far");
    run(list, f, frame_at(100, 32), 1);
    check(list.count() > 0 && list.data()[0].center.x == fx(-36 + 8 + 16 * 2),
          "full parallax layer moves with the camera, offscreen cells culled");
}

void test_repeat() {
    Fixture f;
    VisualLayerSrc l = tile_layer(4, 2);
    l.cells = {1, 3, 4, 5, 1, 3, 4, 5};
    l.repeat = REPEAT_X;
    l.parallax_x = half_of(1);
    bake(f, 4, 2, {l});
    SpriteList list(storage, keys, CAP);
    const LayerDrawStats s = run(list, f, frame_at(100, 32));
    check(s.visited == 9 * 2, "repeat covers the screen with copies, origin 14 gives 9 columns");
    check(list.data()[0].center.x == fx(14 - 16 + 8) && list.data()[0].region == 5,
          "column left of the origin is the last column of the copy before");
    check(list.data()[5].region == 1 && list.data()[5].center.x == fx(14 + 64 + 8),
          "column 4 wraps to column 0");
}

void test_cost_does_not_follow_map_size() {
    Fixture small;
    Fixture large;
    bake(small, 40, 2, {tile_layer(40, 2)});
    bake(large, 400, 2, {tile_layer(400, 2)});
    SpriteList list(storage, keys, CAP);
    const LayerDrawStats a = run(list, small, frame_at(300, 16));
    const LayerDrawStats b = run(list, large, frame_at(300, 16));
    check(a.visited == b.visited && a.visited == 9 * 2, "cost is the window, not the map");
}

void test_snapped_zoom() {
    Fixture f;
    bake(f, 4, 2, {tile_layer(4, 2)});
    SpriteList list(storage, keys, CAP);
    LayerFrame fr = frame_at(0, 0);
    fr.view.zoom = fx(2);
    fr.camera.center = {fix32::from_raw(10 * fix32::ONE + fix32::ONE / 4), fx(0)};
    run(list, f, fr);
    const Sprite& a = list.data()[0];
    check(a.center.x.raw % fix32::ONE == 0 && a.half.x == fx(16),
          "zoomed origin lands on a whole pixel");
    check(list.data()[1].center.x - a.center.x == fx(32), "zoomed cells step by tile size times zoom");
}

void test_runaway_repeat() {
    Fixture f;
    VisualLayerSrc l = tile_layer(4, 2);
    l.repeat = REPEAT_X | REPEAT_Y;
    bake(f, 4, 2, {l});
    LayerFrame fr = frame_at(0, 0);
    fr.view.zoom = fix32::from_raw(fix32::ONE / 32);
    SpriteList list(storage, keys, CAP);
    const LayerDrawStats sub = run(list, f, fr);
    check(sub.truncated && sub.visited == 0, "repeat of a sub-pixel cell is refused, not walked");
    fr.view.zoom = fix32::from_raw(fix32::ONE / 8);
    SpriteList small(storage, keys, 100);
    const LayerDrawStats cut = run(small, f, fr);
    check(cut.truncated && small.count() == 100 && small.dropped() == 1 && cut.visited == 101,
          "a full list stops the walk and counts the sprite it lost");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    test_geometry();
    test_parallax_and_offset();
    test_repeat();
    test_cost_does_not_follow_map_size();
    test_snapped_zoom();
    test_runaway_repeat();
    std::printf("framework-graphics-layer: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
