#include "ui_grid_fixture.hpp"

#include <cstdio>
#include <cstring>

namespace {
using namespace framework::ui;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("ui-grid: FAIL %s\n", why); } }
}
int main() {
    ui_grid_fixture::Scene scene;
    check(scene.init(), "real font and strict strings baking");
    auto& ui = scene.context;
    const UiId grid = 19;
    char value[24]{};
    auto draw = [&](UiInput in, std::string_view key = "grid.name", uint32_t max_len = 3) {
        check(ui.begin(in), "begin");
        const auto result = ui.glyph_grid(grid, key, value, max_len, {0, 0, 120, 60}, 2);
        ui.end(); return result;
    };
    UiInput accept; accept.accept = true; accept.character_count = 1; accept.characters[0] = 'B';
    check(!draw(accept).changed && value[0] == 0, "fresh grid refuses stale accept and typed characters");
    check(ui.focus() == ui_id(grid, "grid.name", 'A'), "first cell focus");
    accept.character_count = 0;
    check(draw(accept).changed && std::string_view(value) == "A", "accept enters focused glyph");
    UiInput right; right.x = 1; right.accept = true;
    check(draw(right).changed && std::string_view(value) == "AB" && ui.focus() == ui_id(grid, "grid.name", 'B'), "horizontal navigation");
    UiInput down; down.y = 1; down.accept = true;
    const auto done = draw(down);
    check(done.done && !done.changed && std::string_view(value) == "AB", "done cell preserves content");
    UiInput finish; finish.done = true; finish.erase = true;
    check(draw(finish).done && std::string_view(value) == "AB", "logical Start completion preserves content before erase");
    UiInput left; left.x = -1; left.accept = true;
    check(draw(left).changed && std::string_view(value) == "A", "erase cell deletes one character");
    check(draw(ui_grid_fixture::tap(70, 5)).changed && std::string_view(value) == "AB", "touch selects known glyph");
    UiInput typed; typed.character_count = 3; typed.characters[0] = 'A'; typed.characters[1] = 'X'; typed.characters[2] = 'B';
    const auto bounded = draw(typed);
    check(bounded.changed && bounded.full && bounded.rejected == 1 && std::string_view(value) == "ABA", "alphabet and scalar length caps");
    UiInput back; back.back = true; back.erase = true; back.characters[0] = 'A'; back.character_count = 1;
    ui.begin(back); const auto closed = ui.glyph_grid(grid, "grid.name", value, 3, {0, 0, 120, 60}, 2); const auto out = ui.end();
    check(closed.back && !closed.changed && !out.back && std::string_view(value) == "ABA", "back belongs to grid and preserves text");
    UiInput cancelled = typed; cancelled.cancelled = true;
    check(!draw(cancelled).changed && std::string_view(value) == "ABA", "cancel suppresses all edits");
    UiInput erase; erase.erase = true;
    check(draw(erase).changed && std::string_view(value) == "AB", "logical erase is independent of back");
    draw(ui_grid_fixture::tap(1, 1)); // A достигает max_len.
    ui.set_language(Language::Ru);
    const auto old = draw(accept, "grid.name", 8);
    check(old.changed && std::string_view(value) == "ABAA" && ui.focus() == ui_id(grid, "grid.name", 'A'),
          "codepoint identity survives alphabet reorder, old A never becomes Cyrillic");
    check(draw(ui_grid_fixture::tap(1, 1), "grid.name", 8).changed && std::string_view(value) == "ABAA\xD0\x96", "Cyrillic touch append");
    check(draw(erase, "grid.name", 8).changed && std::string_view(value) == "ABAA", "whole UTF8 scalar erased");

    value[0] = 0;
    draw({}, "grid.utf8", 3); // Новая сетка сначала должна появиться.
    typed = {}; typed.character_count = 3; typed.characters[0] = 0x416; typed.characters[1] = 0x1f600; typed.characters[2] = 0x20ac;
    check(draw(typed, "grid.utf8", 3).changed && std::string_view(value) == "\xD0\x96\xF0\x9F\x98\x80\xE2\x82\xAC",
          "independent exact two, four and three-byte UTF8 oracle");
    check(draw(erase, "grid.utf8", 3).changed && std::string_view(value) == "\xD0\x96\xF0\x9F\x98\x80", "three-byte erasure");
    check(draw(erase, "grid.utf8", 2).changed && std::string_view(value) == "\xD0\x96", "four-byte erasure");
    check(draw(erase, "grid.utf8", 2).changed && value[0] == 0 && !draw(erase, "grid.utf8", 2).changed, "erase to empty is bounded");
    typed.character_count = 2; typed.characters[0] = 0xd800; typed.characters[1] = 0xffffffff;
    check(draw(typed, "grid.utf8", 2).rejected == 2 && value[0] == 0, "invalid public codepoints refused");
    draw({}, "grid.ip", 12);
    typed = {}; typed.character_count = 4; typed.characters[0] = '0'; typed.characters[1] = '.'; typed.characters[2] = ':'; typed.characters[3] = '1';
    check(draw(typed, "grid.ip", 12).changed && std::string_view(value) == "0.:1", "IP punctuation alphabet");
    value[0] = 0; draw({}, "grid.room", 8);
    typed.characters[0] = 'A'; typed.characters[1] = '0'; typed.characters[2] = 'B'; typed.characters[3] = '1';
    check(draw(typed, "grid.room", 8).changed && std::string_view(value) == "A0B1", "room alphabet");
    typed = {}; typed.x = 1; typed.y = 1;
    draw(typed, "grid.room", 8);
    check(ui.focus() == ui_id(grid, "grid.room", '1'), "two-dimensional navigation on second row");
    draw(typed, "grid.room", 8);
    check(ui.focus() == ui_id(grid, "grid.room", UINT32_MAX - 1), "navigation clamps final row and column");
    draw(typed, "grid.room", 8);
    check(ui.focus() == ui_id(grid, "grid.room", UINT32_MAX - 1), "navigation cannot run beyond cells");
    check(ui.init(scene.strings, scene.fonts, 3, 7), "reset for partial final row");
    ui.begin({}); ui.glyph_grid(grid, "grid.name", value, 8, {0, 0, 120, 60}, 3); ui.end();
    UiInput move; move.x = 1;
    for (int i = 0; i < 2; ++i) { ui.begin(move); ui.glyph_grid(grid, "grid.name", value, 8, {0, 0, 120, 60}, 3); ui.end(); }
    move = {}; move.y = 1;
    ui.begin(move); ui.glyph_grid(grid, "grid.name", value, 8, {0, 0, 120, 60}, 3); ui.end();
    check(ui.focus() == ui_id(grid, "grid.name", UINT32_MAX - 1), "vertical movement clamps a partial final row");
    char other[12]{};
    auto two_grids = [&](UiInput in) {
        ui.begin(in); ui.glyph_grid(1, "grid.name", value, 8, {0, 0, 120, 60}, 2);
        const auto result = ui.glyph_grid(2, "grid.ip", other, 8, {200, 0, 120, 60}, 2); ui.end(); return result;
    };
    two_grids({}); two_grids(ui_grid_fixture::tap(210, 5));
    move = {}; move.x = 1; move.accept = true;
    check(two_grids(move).changed && std::string_view(other) == "01" && ui.focus() == ui_id(2, "grid.ip", '1'),
          "navigation remains in the selected grid, never jumps to a different group");
    std::puts(fails ? "ui-grid: FAIL" : "ui-grid: PASS alphabets, navigation, touch, UTF8 editing and cancellation");
    return fails ? 1 : 0;
}
