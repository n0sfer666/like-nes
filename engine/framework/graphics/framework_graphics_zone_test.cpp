#include <cstdio>
#include <string>

#include "hash.hpp"
#include "layer_cover_fixture.hpp"
#include "viewport_fit.hpp"

namespace {

using namespace layer_cover_fixture;
namespace gr = framework::graphics;
constexpr gr::PixelSize ZONE{640, 360};
constexpr gr::PixelSize LIMIT{960, 540};
using Cover = bool (*)(const VisualMapSrc&, const ObjectMapSrc&, std::string&, gr::PixelSize, gr::PixelSize);

VisualMapSrc scene() {
    VisualLayerSrc sky = image_layer("sky", 64, LIMIT.h);
    sky.repeat = tilemap::REPEAT_X;
    sky.parallax_x = sky.parallax_y = fix32{};
    sky.offset_y = fix32::from_int(-270);
    VisualMapSrc map = map_of({tile_layer("street"), sky});
    map.width = 100;
    map.height = 60;
    return map;
}

bool old_limit(const VisualMapSrc& map, const ObjectMapSrc& objects, std::string& error,
               gr::PixelSize zone, gr::PixelSize) {
    return gr::check_layer_cover(map, objects, error, zone);
}

bool old_zone(const VisualMapSrc& map, const ObjectMapSrc& objects, std::string& error,
              gr::PixelSize, gr::PixelSize limit) {
    return gr::check_layer_cover(map, objects, error, gr::VIEW_ZONE, limit);
}

bool covers(Cover read) {
    std::string error;
    return read(scene(), bounds(168, 98, 1432, 862), error, ZONE, LIMIT);
}

bool refuses_short_sky(Cover read) {
    VisualMapSrc map = scene();
    --map.layers[1].image_h;
    std::string error;
    return !read(map, bounds(168, 98, 1432, 862), error, ZONE, LIMIT) &&
           error == gap("sky", "y short by 1 px bottom");
}

uint64_t coverage_hash() {
    uint64_t h = asset::FNV_OFFSET;
    VisualMapSrc map = scene();
    // Все четыре края и один короткий фон: голден содержит вердикт и точный текст зазора.
    const ObjectMapSrc cases[] = {bounds(168, 98, 1432, 862), bounds(167, 98, 1432, 862),
        bounds(168, 97, 1432, 862), bounds(168, 98, 1433, 862), bounds(168, 98, 1432, 863)};
    for (const ObjectMapSrc& objects : cases) {
        std::string error;
        h = asset::fnv1a_u32(h, gr::check_layer_cover(map, objects, error, ZONE, LIMIT) ? 1 : 0);
        h = asset::fnv1a_str(error.c_str(), h);
    }
    --map.layers[1].image_h;
    std::string error;
    h = asset::fnv1a_u32(h, gr::check_layer_cover(map, cases[0], error, ZONE, LIMIT) ? 1 : 0);
    return asset::fnv1a_str(error.c_str(), h);
}

void cover_limits() {
    VisualMapSrc map = scene();
    map.layers[1].image_h = 359;
    map.layers[1].offset_y = fix32::from_int(-180);
    std::string error;
    check(!gr::check_layer_cover(map, bounds(8, 8, 1592, 952), error, ZONE, {1, 1}) &&
          error == gap("sky", "y short by 1 px bottom"),
          "bake clamps a narrow limit to the zone exactly like the renderer");
    check(!gr::check_layer_cover(map, FIT, error, {0, 360}, LIMIT) &&
          error == "map 'lv': viewport zone has a zero side", "a zero zone is refused");
}

void viewport() {
    const gr::ViewportFit wide = gr::viewport_fit({3840, 1080}, ZONE, LIMIT);
    check(wide.scale == 3 && wide.visible.w == 960 && wide.visible.h == 360 && wide.shown.w == 2880,
          "a wide screen uses the caller's zone and horizontal limit");
    const gr::ViewportFit tall = gr::viewport_fit({1920, 2160}, ZONE, LIMIT);
    check(tall.scale == 3 && tall.visible.w == 640 && tall.visible.h == 540 && tall.shown.h == 1620,
          "a tall screen uses the caller's vertical limit");
    const gr::ViewportFit clamped = gr::viewport_fit({3840, 1080}, ZONE, {1, 1});
    check(clamped.visible.w == 640 && clamped.visible.h == 360,
          "a limit narrower than the zone cannot crop the simulated zone");
    const gr::ViewportFit cropped = gr::viewport_fit({320, 180}, ZONE, LIMIT);
    check(cropped.cropped && cropped.scale == 1 && cropped.visible.w == 320 && cropped.visible.h == 180,
          "a screen smaller than a custom zone reports its crop");
}

} // namespace

int main() {
    viewport();
    cover_limits();
    check(covers(gr::check_layer_cover) && !covers(old_zone),
          "control: the old zone refuses layers that cover the custom zone's full camera range");
    check(refuses_short_sky(gr::check_layer_cover) && !refuses_short_sky(old_limit),
          "control: a layer one pixel short must be refused under the custom limit");
    const uint64_t hash = coverage_hash();
    check(hash == 0xfa700b97d63153f4ull, "custom zone coverage keeps its golden");
    std::printf("zone 640x360 coverage hash = 0x%016llx\n", static_cast<unsigned long long>(hash));
    std::printf("framework-graphics-zone: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
