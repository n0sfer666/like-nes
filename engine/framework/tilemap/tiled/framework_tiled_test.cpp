#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "tiled_fixture.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::tilemap;
using framework::tiled::Level;
using tiled_fixture::MemSource;

constexpr int32_t ONE = 65536;

void test_sources(const MemSource& src) {
    const auto asked = [&](const char* line) {
        return std::find(src.asked.begin(), src.asked.end(), line) != src.asked.end();
    };
    check(asked("levels/one.tmj -> ../art/tiles.tsj"), "external tileset asked relative to the map");
    check(asked("art/tiles.tsj -> city.png"), "tileset image asked relative to the tileset file");
    check(asked("levels/one.tmj -> deco.png"), "embedded tileset image asked relative to the map");
    check(asked("levels/one.tmj -> ../art/sky.png"), "image layer asked relative to the map");
}

void test_collision(const Level& level) {
    std::vector<uint8_t> tiled, pipe;
    MapBakeError e1, e2;
    const std::vector<ParsedMap> maps{level.collision};
    check(bake_maps(maps, tiled, e1), "collision from Tiled bakes");
    check(bake_maps(tiled_fixture::PIPE_MAP, pipe, e2), "equivalent pipe map bakes");
    check(!tiled.empty() && tiled == pipe, "collision LNTM from Tiled equals LNTM from the pipe map byte for byte");
    if (!e1.message.empty() || !e2.message.empty())
        std::printf("    %s | %s\n", e1.message.c_str(), e2.message.c_str());
}

void test_visual(const VisualMapSrc& v) {
    check(v.name == "one" && v.width == 4 && v.height == 3 && v.tile_size == 16, "visual map keeps its size");
    check(v.background_rgba == 0x11223380u, "#AARRGGBB background turns into RGBA");
    check(v.tilesets.size() == 2, "both tilesets imported");
    if (v.tilesets.size() == 2) {
        const VisualTileset& c = v.tilesets[0];
        const VisualTileset& d = v.tilesets[1];
        check(c.texture_guid == tiled_fixture::CITY && c.first_index == 1 && c.tile_count == 8 && c.columns == 4,
              "external tileset keeps image guid and grid");
        check(d.texture_guid == tiled_fixture::DECO && d.first_index == 9 && d.tile_count == 2 && d.columns == 2,
              "embedded tileset follows the first one");
    }
    check(v.anims.size() == 1, "one animated tile");
    if (v.anims.size() == 1) {
        const VisualAnimSrc& a = v.anims[0];
        check(a.index == 6 && a.frames.size() == 2, "animation sits on tile index 6");
        check(a.frames.size() == 2 && a.frames[0].index == 7 && a.frames[0].ticks == 6 && a.frames[1].index == 8 &&
                  a.frames[1].ticks == 16,
              "frame ids and milliseconds turn into indices and rounded ticks");
    }
    check(v.layers.size() == 3, "sky, collision copy and deco; a visible layer in a hidden group is dropped");
    if (v.layers.size() != 3) return;
    const VisualLayerSrc& sky = v.layers[0];
    const VisualLayerSrc& solid = v.layers[1];
    const VisualLayerSrc& deco = v.layers[2];
    check(sky.name == "sky" && sky.kind == LayerKind::Image && sky.image_guid == tiled_fixture::SKY &&
              sky.image_w == 128 && sky.image_h == 64,
          "image layer takes the texture guid and size from the source");
    check(sky.repeat == REPEAT_X && sky.offset_x.raw == 4 * ONE, "image layer keeps repeat and offset");
    const std::vector<uint16_t> want_solid{0, 0, 3, 0, 0, static_cast<uint16_t>(2 | CELL_FLIP_H), 1, 4, 1, 1, 1, 1};
    check(solid.name == "solid" && solid.cells == want_solid, "visible_too copies the collision layer with flips");
    check(solid.offset_x.raw == 16 * ONE && solid.offset_y.raw == -32 * ONE, "group offset adds to layer offset");
    const std::vector<uint16_t> want_deco{6, 0, 0, 0, 0, static_cast<uint16_t>(9 | CELL_FLIP_V | CELL_FLIP_D), 0, 0,
                                          10, 0, 0, 5};
    check(deco.cells == want_deco, "deco cells map gids to indices across tilesets");
    check(deco.parallax_x.raw == ONE / 4 && deco.parallax_y.raw == ONE && deco.opacity == 64,
          "group parallax and opacity multiply into the layer");
    check(deco.offset_x.raw == 0 && deco.offset_y.raw == 8 * ONE, "a sibling group offset stays with its group");
    check(deco.repeat == REPEAT_Y && solid.repeat == 0, "tile layer repeat comes from its repeat_y property");
    check(!sky.cover_y && !deco.cover_y && solid.cover_y, "cover_y on image and tile layers, true by default");
}

void test_objects(const ObjectMapSrc& o) {
    check(o.name == "one" && o.objects.size() == 3, "three objects imported");
    if (o.objects.size() != 3) return;
    const ObjectSrc& start = o.objects[0];
    const ObjectSrc& door = o.objects[1];
    const ObjectSrc& pit = o.objects[2];
    check(start.shape == ObjectShape::Point && start.cls == "spawn" && start.x.raw == 12 * ONE &&
              start.y.raw == 20 * ONE,
          "point takes the layer offset");
    check(start.props.size() == 2 && start.props[0].type == PropType::Int && start.props[0].value == 3 &&
              start.props[1].type == PropType::Fix && start.props[1].value == ONE * 3 / 2,
          "int and float properties");
    check(door.shape == ObjectShape::Rect && door.x.raw == 34 * ONE && door.w.raw == 16 * ONE &&
              door.h.raw == 32 * ONE,
          "rectangle keeps its size");
    check(door.props.size() == 2 && door.props[0].text == "two" && door.props[1].type == PropType::Bool &&
              door.props[1].value == 1,
          "string and bool properties");
    check(pit.shape == ObjectShape::Polygon && pit.cls.empty() && pit.vertices.size() == 3 &&
              pit.vertices[1].x_raw == 16 * ONE && pit.vertices[2].y_raw == 8 * ONE,
          "polygon keeps its vertices");
}

void test_sections(const Level& level) {
    std::vector<uint8_t> visual, objects;
    std::string error;
    check(bake_visuals(std::vector<VisualMapSrc>{level.visual}, visual, error), "imported visual map bakes");
    check(bake_objects(std::vector<ObjectMapSrc>{level.objects}, objects, error), "imported objects bake");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
}

void test_symmetric_flips(const Level& level) {
    std::string tmj = tiled_fixture::TMJ;
    for (const auto& [from, to] : {std::pair<std::string, std::string>{"0,0,3,0,", "0,0,1073741827,0,"},
                                   {"1,1,1,1]", "1,2147483649,3221225473,1]"},
                                   {"[6,0,0,0,", "[6,2147483648,0,0,"}})
        tmj.replace(tmj.find(from), from.size(), to);
    MemSource src;
    Level flipped;
    std::string error;
    check(tiled_fixture::import(tmj, src, flipped, error), "flipped solid, one-way and empty cells import");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(flipped.collision.flags == level.collision.flags, "flips leave solid and one-way collision as is");
    check(flipped.visual.layers.size() == 3 && flipped.visual.layers[2].cells == level.visual.layers[2].cells,
          "a flipped empty cell stays empty");
}

void test_image_cover_default() {
    std::string tmj = tiled_fixture::TMJ;
    const std::string from = R"(,"properties":[{"name":"cover_y","type":"bool","value":false}]})";
    tmj.replace(tmj.find(from), from.size(), "}");
    MemSource src;
    Level plain;
    std::string error;
    check(tiled_fixture::import(tmj, src, plain, error), "sky without properties imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(!plain.visual.layers.empty() && plain.visual.layers[0].cover_y,
          "an image layer without cover_y is judged on y");
}

} // namespace

int main() {
    MemSource src;
    Level level;
    std::string error;
    check(tiled_fixture::import(tiled_fixture::TMJ, src, level, error), "fixture map imports");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    test_sources(src);
    test_collision(level);
    test_visual(level.visual);
    test_objects(level.objects);
    test_sections(level);
    test_symmetric_flips(level);
    test_image_cover_default();
    std::printf("framework-tiled: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
