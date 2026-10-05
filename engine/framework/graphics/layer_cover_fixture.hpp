#pragma once
#include <cstdio>
#include <string>

#include "layer_cover.hpp"

namespace layer_cover_fixture {

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::tilemap;

inline VisualLayerSrc tile_layer(const char* name) {
    VisualLayerSrc l;
    l.name = name;
    return l;
}

inline VisualLayerSrc image_layer(const char* name, uint32_t w, uint32_t h) {
    VisualLayerSrc l = tile_layer(name);
    l.kind = LayerKind::Image;
    l.image_w = w;
    l.image_h = h;
    return l;
}

inline VisualMapSrc map_of(std::initializer_list<VisualLayerSrc> layers) {
    VisualMapSrc m;
    m.name = "lv";
    m.width = 40;
    m.height = 21;
    m.tile_size = 16;
    m.layers = layers;
    return m;
}

inline ObjectSrc rect(const char* cls, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
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

inline ObjectMapSrc bounds(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    ObjectMapSrc m;
    m.name = "lv";
    m.objects.push_back(rect("spawn", 0, 0, 1, 1));
    m.objects.push_back(rect("bounds", x0, y0, x1, y1));
    return m;
}

inline const ObjectMapSrc FIT = bounds(104, 56, 536, 280);

inline void passes(const VisualMapSrc& m, const ObjectMapSrc& o, const char* what) {
    std::string error;
    const bool ok = framework::graphics::check_layer_cover(m, o, error);
    if (!ok) std::printf("  refused: %s\n", error.c_str());
    check(ok, what);
}

inline std::string gap(const char* layer, const char* what) {
    return std::string("map 'lv': layer '") + layer + "' does not cover the view: " + what;
}

inline void refuses(const VisualMapSrc& m, const ObjectMapSrc& o, const std::string& reason,
                    const char* what) {
    std::string error;
    const bool ok = framework::graphics::check_layer_cover(m, o, error);
    const bool right = !ok && error == reason;
    if (!right)
        std::printf("  got %s: %s\n  want: %s\n", ok ? "pass" : "refusal", error.c_str(),
                    reason.c_str());
    check(right, what);
}

} // namespace layer_cover_fixture
