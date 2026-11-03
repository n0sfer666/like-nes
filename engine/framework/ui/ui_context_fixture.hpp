#pragma once
#include <string>
#include <vector>

#include "font_fixture.hpp"
#include "strings_bake.hpp"
#include "ui_context.hpp"

namespace ui_context_fixture {
struct Scene {
    std::vector<uint8_t> font_bytes, string_bytes;
    framework::graphics::FontTable fonts;
    framework::ui::Strings strings;
    framework::ui::Context context;
    bool init(bool large = false) {
        auto font = font_fixture::small(); font.name = "mono";
        std::string error;
        if (!framework::graphics::bake_fonts({&font, 1}, font_bytes, error) || !fonts.open(font_bytes.data(), font_bytes.size())) return false;
        std::vector<framework::ui::StringSrc> rows{{"one", "A", "\xD0\x96"}, {"two", "AA", "\xD0\x96\xD0\x96"},
            {"volume", "?A", "?\xD0\x96"}, {"name", "A?", "\xD0\x96?"}};
        if (large) rows.push_back({"long", std::string(1u << 20, 'A'), "A"});
        framework::ui::StringsStats stats;
        return framework::ui::bake_strings(rows, font, true, string_bytes, stats, error) &&
               strings.open(string_bytes.data(), string_bytes.size()) && context.init(strings, fonts, 3, 7);
    }
};
}
