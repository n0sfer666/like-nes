#pragma once
#include <string>
#include <vector>

#include "font_bake.hpp"
#include "strings_bake.hpp"
#include "ui_context.hpp"
#include "ui_edit.hpp"

namespace ui_grid_fixture {
struct Scene {
    std::vector<uint8_t> font_bytes, string_bytes;
    framework::graphics::FontTable fonts;
    framework::ui::Strings strings;
    framework::ui::Context context;
    bool init() {
        framework::graphics::FontSrc font;
        font.name = "mono"; font.line_height = 8; font.page_w = font.page_h = 8;
        for (uint32_t cp = 32; cp < 127; ++cp) font.glyphs.push_back({cp, 0, 0, 2, 4});
        for (uint32_t cp = 0x100; cp < 0x180; ++cp) font.glyphs.push_back({cp, 0, 0, 2, 4});
        for (uint32_t cp : {0x416u, 0x20acu, 0x1f600u}) font.glyphs.push_back({cp, 0, 0, 2, 4});
        std::string full, too_many;
        for (uint32_t cp = 0x100; cp < 0x17f; ++cp) {
            std::array<char, 4> bytes; const auto size = framework::ui::edit::encode(cp, bytes);
            too_many.append(bytes.data(), size);
            if (cp < 0x17e) full.append(bytes.data(), size);
        }
        const framework::ui::StringSrc rows[]{
            {"grid.name", "AB", "\xD0\x96" "A"}, {"grid.ip", "01.:", "01.:"},
            {"grid.room", "AB01", "AB01"},
            {"grid.utf8", "\xD0\x96\xF0\x9F\x98\x80\xE2\x82\xAC", "\xD0\x96\xF0\x9F\x98\x80\xE2\x82\xAC"},
            {"grid.erase", "A?", "A?"}, {"grid.done", "?A", "?A"}, {"one", "A", "A"},
            {"duplicate", "AA", "AA"}, {"full", full, full}, {"too-many", too_many, too_many},
            {"long", std::string(4096, 'A'), "A"}};
        std::string error; framework::ui::StringsStats stats;
        return framework::graphics::bake_fonts({&font, 1}, font_bytes, error) && fonts.open(font_bytes.data(), font_bytes.size()) &&
            framework::ui::bake_strings(rows, font, true, string_bytes, stats, error) && strings.open(string_bytes.data(), string_bytes.size()) &&
            context.init(strings, fonts, 3, 7);
    }
};
inline framework::ui::UiInput tap(float x, float y) {
    framework::ui::UiInput in; in.pointer_count = 2;
    in.pointers[0] = {INTPTR_MAX, framework::ui::UiPointerPhase::Down, x, y};
    in.pointers[1] = {INTPTR_MAX, framework::ui::UiPointerPhase::Up, x, y}; return in;
}
}
