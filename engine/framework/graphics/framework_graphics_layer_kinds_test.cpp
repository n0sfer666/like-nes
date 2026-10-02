#include <cstdio>

#include "layer_fixture.hpp"
#include "platform_args.hpp"
#include "visual_texels.hpp"

namespace {

using namespace framework::graphics;
using namespace framework::tilemap;
using namespace layer_fixture;

void test_animation() {
    Fixture f;
    VisualLayerSrc l = tile_layer(1, 1);
    l.cells = {static_cast<uint16_t>(2 | CELL_FLIP_V)};
    bake(f, 1, 1, {l});
    SpriteList list(storage, keys, CAP);
    LayerFrame fr = frame_at(64, 32);
    const uint16_t want[8] = {5, 5, 5, 6, 6, 6, 6, 5};
    bool ok = true;
    for (uint64_t t = 0; t < 8; ++t) {
        fr.tick = t;
        run(list, f, fr);
        const Sprite& s = list.data()[0];
        ok = ok && list.count() == 1 && s.region == want[t] && s.flip == SPRITE_FLIP_V;
    }
    check(ok, "animated cell shows the frame of the tick and keeps its flip");
}

void test_image_and_unknown() {
    Fixture f;
    VisualLayerSrc sky;
    sky.name = "sky";
    sky.kind = LayerKind::Image;
    sky.repeat = REPEAT_X | REPEAT_Y;
    sky.image_guid = TEX_SKY;
    sky.image_w = 64;
    sky.image_h = 32;
    bake(f, 4, 2, {sky, tile_layer(4, 2)});
    SpriteList list(storage, keys, CAP);
    const LayerDrawStats s = run(list, f, frame_at(64, 32));
    check(s.emitted == 4 && list.data()[3].region == 0 && list.data()[3].material == 2,
          "repeated image layer is one whole-texture quad per copy");
    const Sprite& last = list.data()[3];
    check(last.center == Vec2{fx(96), fx(48)} && last.half == Vec2{fx(32), fx(16)},
          "image copy sits on the image grid");
    LayerFrame fr = frame_at(64, 32);
    const uint64_t only_b[1] = {TEX_B};
    fr.textures = only_b;
    const LayerDrawStats u = run(list, f, fr, 1);
    check(u.unknown == 8 && u.emitted == 0, "cells of an unregistered texture are counted, not drawn");
}

void test_texels() {
    Fixture f;
    bake(f, 1, 1, {tile_layer(1, 1)});
    TexelRect r;
    check(tile_texels(f.map, 7, r) && r.x == 32 && r.y == 16 && r.w == 16, "texels of a plain sheet");
    check(tile_texels(f.map, 12, r) && r.x == 19 && r.y == 19, "texels honour margin and spacing");
    check(tile_texels(f.map, 10, r) && r.x == 19 && r.y == 1, "margin alone above the first row");
    check(tile_texels(f.map, 11, r) && r.x == 1 && r.y == 19, "margin alone left of the first column");
    check(!tile_texels(f.map, 0, r) && !tile_texels(f.map, 13, r), "foreign indices have no texels");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    test_animation();
    test_image_and_unknown();
    test_texels();
    std::printf("framework-graphics-layer-kinds: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
