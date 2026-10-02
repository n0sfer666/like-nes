#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "hash_mix.hpp"
#include "visual_bake.hpp"
#include "visual_read.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::tilemap;

constexpr uint64_t GOLDEN = 0x8f7393b94f0acdddull;

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = framework::physics::FNV_OFFSET;
    framework::physics::mix_bytes(h, b.data(), b.size());
    return h;
}

std::vector<VisualMapSrc> source() {
    VisualMapSrc one;
    one.name = "one";
    one.width = 3;
    one.height = 2;
    one.tile_size = 16;
    one.background_rgba = 0x112233FFu;
    one.tilesets = {VisualTileset{0xA1, 1, 4, 2, 0, 0, 0}, VisualTileset{0xB2, 5, 2, 1, 1, 2, 0}};
    one.anims = {VisualAnimSrc{6, {{5, 3}}}, VisualAnimSrc{2, {{3, 5}, {4, 10}}}};
    VisualLayerSrc tiles;
    tiles.name = "ground";
    tiles.opacity = 128;
    tiles.repeat = REPEAT_X;
    tiles.parallax_x = fix32(32768, 0);
    tiles.offset_x = fix32::from_int(-8);
    tiles.offset_y = fix32::from_int(4);
    tiles.cells = {1, static_cast<uint16_t>(2 | CELL_FLIP_H), 0, 5,
                   static_cast<uint16_t>(6 | CELL_FLIP_V | CELL_FLIP_D), 3};
    VisualLayerSrc sky;
    sky.name = "sky";
    sky.kind = LayerKind::Image;
    sky.repeat = REPEAT_X | REPEAT_Y;
    sky.image_guid = 0xC3;
    sky.image_w = 64;
    sky.image_h = 32;
    one.layers = {sky, tiles};
    VisualMapSrc two;
    two.name = "two";
    two.width = 1;
    two.height = 1;
    two.tile_size = 8;
    two.tilesets = {VisualTileset{0xD4, 1, 1, 1, 0, 0, 0}};
    VisualLayerSrc only;
    only.name = "only";
    only.cells = {1};
    two.layers = {only};
    return {one, two};
}

void test_round_trip(const std::vector<uint8_t>& bytes) {
    VisualTable t;
    check(t.open(bytes.data(), bytes.size()), "baked table opens");
    check(t.count() == 2 && std::strcmp(t.name(1), "two") == 0, "both maps keep their names");
    const VisualMap m = t.find("one");
    check(m.row != nullptr && t.find("absent").row == nullptr, "find hits by name and misses unknown");
    if (m.row == nullptr) return;
    check(m.row->width == 3 && m.row->height == 2 && m.row->tile_size == 16, "map keeps its size");
    check(m.row->background_rgba == 0x112233FFu, "map keeps its background");
    check(m.tilesets.size() == 2 && m.tilesets[1].texture_guid == 0xB2 && m.tilesets[1].margin == 1 &&
              m.tilesets[1].spacing == 2 && m.tilesets[1].first_index == 5,
          "tilesets keep guid, margin, spacing and first index");
    check(m.anims.size() == 2 && m.anims[0].index == 2 && m.anims[1].index == 6, "animations sorted by index");
    check(m.anims[0].cycle_ticks == 15, "cycle is the sum of frame ticks");
    check(m.layers.size() == 2 && m.layers[0].kind == static_cast<uint8_t>(LayerKind::Image), "layer order kept");
    if (m.layers.size() != 2) return;
    const VisualLayer& sky = m.layers[0];
    const VisualLayer& ground = m.layers[1];
    check(std::strcmp(layer_name(m, ground), "ground") == 0, "layer name survives");
    check(sky.image_guid == 0xC3 && sky.image_w == 64 && sky.image_h == 32 && layer_cells(m, sky).empty(),
          "image layer keeps its image and has no cells");
    check(sky.repeat == (REPEAT_X | REPEAT_Y) && ground.repeat == REPEAT_X, "repeat flags survive");
    check(ground.opacity == 128 && ground.parallax_x_raw == 32768 && ground.parallax_y_raw == 65536,
          "opacity and parallax survive");
    check(ground.offset_x_raw == -8 * 65536 && ground.offset_y_raw == 4 * 65536, "offset survives");
    const auto cells = layer_cells(m, ground);
    check(cells.size() == 6 && cells[4] == (6 | CELL_FLIP_V | CELL_FLIP_D), "cells keep their flip bits");
    const uint16_t flipped = static_cast<uint16_t>(2 | CELL_FLIP_H);
    check(visual_region(m, flipped, 0) == (3 | CELL_FLIP_H), "animated cell shows frame 0 with its flip");
    check(visual_region(m, flipped, 4) == (3 | CELL_FLIP_H), "frame 0 lasts its ticks");
    check(visual_region(m, flipped, 5) == (4 | CELL_FLIP_H), "frame 1 starts after frame 0");
    check(visual_region(m, flipped, 15) == (3 | CELL_FLIP_H), "cycle wraps");
    check(visual_region(m, 1, 7) == 1 && visual_region(m, 0, 7) == 0, "static and empty cells stay");
    check(tileset_of(m, 6) == &m.tilesets[1] && tileset_of(m, 4) == &m.tilesets[0], "tileset of a cell");
    check(tileset_of(m, 7) == nullptr && tileset_of(m, 0) == nullptr, "no tileset past the end or for empty");
}

struct Bad {
    const char* what;
    std::function<void(std::vector<VisualMapSrc>&)> edit;
    const char* message;
};

void test_bake_refusals() {
    const Bad cases[] = {
        {"duplicate map", [](auto& m) { m[1].name = "one"; }, "declared twice"},
        {"unnamed map", [](auto& m) { m[1].name.clear(); }, "needs a name"},
        {"cells for another size", [](auto& m) { m[0].layers[1].cells.pop_back(); }, "different size"},
        {"cells on an image layer", [](auto& m) { m[0].layers[0].cells = {1}; }, "different size"},
        {"cell past the tilesets", [](auto& m) { m[1].layers[0].cells = {2}; }, ""},
        {"flipped empty cell", [](auto& m) { m[0].layers[1].cells[2] = CELL_FLIP_H; }, ""},
    };
    for (const Bad& c : cases) {
        std::vector<VisualMapSrc> src = source();
        c.edit(src);
        std::vector<uint8_t> out;
        std::string error;
        const bool ok = bake_visuals(src, out, error);
        check(!ok && error.find(c.message) != std::string::npos, c.what);
        if (ok || error.find(c.message) == std::string::npos) std::printf("    got: %s\n", error.c_str());
    }
}

void test_reader_refusals(const std::vector<uint8_t>& bytes) {
    VisualTable t;
    t.open(bytes.data(), bytes.size());
    const VisualMap m = t.find("one");
    if (m.row == nullptr || m.layers.size() != 2) return;
    const auto at = [&](const void* p) {
        return static_cast<std::size_t>(static_cast<const uint8_t*>(p) - bytes.data());
    };
    const auto refused = [&](const char* what, std::size_t offset, std::vector<uint8_t> patch) {
        std::vector<uint8_t> bad = bytes;
        std::memcpy(bad.data() + offset, patch.data(), patch.size());
        VisualTable r;
        check(!r.open(bad.data(), bad.size()), what);
    };
    refused("wrong magic", 0, {'L', 'N', 'T', 'M'});
    refused("cell past the tilesets", at(layer_cells(m, m.layers[1]).data()), {7, 0});
    refused("flipped empty cell", at(layer_cells(m, m.layers[1]).data() + 2), {0, 0x80});
    refused("unknown layer kind", at(&m.layers[1].kind), {2});
    refused("repeat bits past X|Y", at(&m.layers[1].repeat), {4});
    refused("image layer with cells", at(&m.layers[0].cells_offset), {16, 0, 0, 0});
    refused("image layer of zero width", at(&m.layers[0].image_w), {0, 0, 0, 0});
    refused("frame of zero ticks", at(&m.frames[0].ticks), {0, 0});
    refused("cycle not the sum of frames", at(&m.anims[0].cycle_ticks), {16, 0, 0, 0});
    refused("animations out of order", at(&m.anims[1].index), {1, 0});
    refused("tilesets not contiguous", at(&m.tilesets[1].first_index), {6, 0, 0, 0});
    std::vector<uint8_t> cut(bytes.begin(), bytes.end() - 16);
    VisualTable r;
    check(!r.open(cut.data(), cut.size()), "truncated section");
}

} // namespace

int main() {
    std::vector<uint8_t> bytes;
    std::string error;
    check(bake_visuals(source(), bytes, error), "visual maps bake");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    test_round_trip(bytes);
    const uint64_t h = hash_bytes(bytes);
    check(h == GOLDEN, "byte table matches the golden");
    if (h != GOLDEN) std::printf("    hash 0x%016llx\n", static_cast<unsigned long long>(h));
    test_bake_refusals();
    test_reader_refusals(bytes);
    std::printf("framework-tilemap-visual: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
