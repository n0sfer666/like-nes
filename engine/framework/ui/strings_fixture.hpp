#pragma once
// Test/fuzz data, not part of the SDK.
#include "font_fixture.hpp"
#include "strings_bake.hpp"

namespace strings_fixture {
inline framework::graphics::FontSrc font() {
    auto f = font_fixture::small();
    f.name = "mono";
    for (uint32_t cp = 33; cp < 127; ++cp)
        if (cp != 'A' && cp != '?') f.glyphs.push_back({cp, 0, 0, 1, 1});
    return f;
}
inline std::vector<framework::ui::StringSrc> rows() { return {{"go", "Go", ""}, {"name", "A", "\xD0\x96"}}; }
}
