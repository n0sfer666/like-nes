#include "ui_grid_fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

namespace {
using namespace framework::ui;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("ui-grid-limits: FAIL %s\n", why); } }
}
int main() {
    ui_grid_fixture::Scene scene;
    check(scene.init(), "font covers large and UTF8 alphabets");
    auto& ui = scene.context;
    char value[12] = "A";
    auto grid = [&](std::span<char> buffer, std::string_view key = "grid.name", uint32_t max_len = 8,
                    UiRect rect = {0, 0, 120, 60}, uint32_t columns = 2, UiGridLabels labels = {}) {
        return ui.glyph_grid(0, key, buffer, max_len, rect, columns, labels);
    };
    check(grid(value).invalid && std::string_view(value) == "A", "outside frame refused");
    auto invalid = [&](std::span<char> buffer, std::string_view key, uint32_t max_len, UiRect rect, uint32_t columns) {
        ui.begin({}); const auto result = grid(buffer, key, max_len, rect, columns); const auto out = ui.end();
        check(result.invalid && !result.changed && out.quads.empty(), "invalid grid refuses before painting/editing");
    };
    invalid(value, "duplicate", 8, {0, 0, 120, 60}, 2);
    invalid(value, "too-many", 8, {0, 0, 512, 288}, 16);
    invalid(value, "missing", 8, {0, 0, 120, 60}, 2);
    invalid(value, "grid.name", UINT32_MAX, {0, 0, 120, 60}, 2);
    invalid(value, "grid.name", 8, {0, 0, 120, 60}, 0);
    invalid(value, "grid.name", 8, {INT32_MAX, 0, 120, 60}, 2);
    invalid(value, "grid.name", 8, {-1, 0, 120, 60}, 2);
    invalid(value, "grid.name", 8, {500, 0, 120, 60}, 2);
    invalid(value, "grid.name", 8, {0, 0, 12, 8}, 2);
    invalid({}, "grid.name", 8, {0, 0, 120, 60}, 2);
    char bad[] = "\xC0\xAF";
    invalid(bad, "grid.name", 8, {0, 0, 120, 60}, 2);
    check(static_cast<unsigned char>(bad[0]) == 0xc0, "invalid UTF8 not overwritten");
    std::array<char, UI_FIELD_BYTES + 1> unterminated; unterminated.fill('A'); unterminated.back() = 0;
    invalid(unterminated, "grid.name", 8, {0, 0, 120, 60}, 2);
    ui.begin({}); check(grid(value, "grid.name", 8, {0, 0, 120, 60}, 2, {"missing", "grid.done"}).invalid, "missing control label"); ui.end();
    ui.begin({}); const auto full = grid(value, "full", 8, {0, 0, 512, 288}, 16); auto out = ui.end();
    check(!full.invalid && out.stats.widgets == 128 && out.stats.dropped_widgets == 0 && out.stats.unknown_glyphs == 0,
          "126 scalars plus two controls exactly fit widget capacity");
    UiInput typed; typed.character_count = 1; typed.characters[0] = 'B';
    ui.begin({}); grid(value); ui.end();
    ui.begin(typed); const auto limited = grid(value, "grid.name", 0); ui.end();
    check(limited.full && !limited.changed && std::string_view(value) == "A", "zero scalar cap preserves existing content");
    UiInput erase; erase.erase = true;
    ui.begin(erase); check(grid(value, "grid.name", 0).changed && value[0] == 0, "erase repairs existing over-limit text"); ui.end();
    char short_buffer[4]{};
    ui.begin({}); grid(short_buffer, "grid.utf8"); ui.end();
    typed.characters[0] = 0x1f600;
    ui.begin(typed); const auto no_bytes = grid(short_buffer, "grid.utf8"); ui.end();
    check(no_bytes.full && !no_bytes.changed && short_buffer[0] == 0, "four bytes need fifth byte for NUL");
    typed.characters[0] = 0x416;
    ui.begin(typed); check(grid(short_buffer, "grid.utf8").changed && std::strlen(short_buffer) == 2, "two bytes plus NUL fit"); ui.end();
    ui.begin(typed); check(grid(short_buffer, "grid.utf8").full && std::strlen(short_buffer) == 2, "byte capacity separate from scalar limit"); ui.end();

    value[0] = 0;
    ui.begin({}); grid(value); ui.end();
    typed.characters[0] = 'A';
    ui.begin(typed); const auto first = grid(value); const auto duplicate = grid(value); out = ui.end();
    check(first.changed && duplicate.invalid && std::string_view(value) == "A" && out.stats.duplicate_ids == 1, "duplicate group cannot edit twice");
    ui.begin({}); grid(value);
    check(grid(value, "grid.ip", 8, {200, 0, 120, 60}).invalid, "same parent with different alphabet is still a duplicate group"); ui.end();
    ui.begin({}); ui.button(ui_id(0, "grid.name", 'A'), "one", {200, 0, 20, 20});
    check(grid(value).invalid, "cell collision with existing widget refused"); ui.end();
    ui.begin({}); ui.button(ui_id(0, "grid.name", 'A'), "one", {0, 0, 20, 20}); ui.end();
    ui.begin(typed); check(!grid(value).changed && std::string_view(value) == "A", "old ordinary button ID cannot arm a new grid"); ui.end();
    ui.begin({}); for (uint32_t i = 0; i < 125; ++i) ui.button(i + 100, "one", {200, 0, 20, 20});
    const auto no_widgets = grid(value); out = ui.end();
    check(no_widgets.invalid && out.stats.widgets == 125, "partial widget capacity refuses whole grid before registration");
    ui.begin({}); grid(value); ui.end();
    ui.begin(typed);
    // Узкие длинные кнопки заполняют2048квадов до ранее видимой сетки.
    for (uint32_t i = 0; i < 12; ++i) ui.button(i + 100, "long", {200, 0, 32, 288});
    const auto overflow = grid(value); out = ui.end();
    check(overflow.invalid && !overflow.changed && std::string_view(value) == "A" && out.stats.dropped_quads > 0, "undrawn grid cannot edit caller data");
    ui.begin(typed); check(!grid(value).changed && std::string_view(value) == "A", "failed previous grid needs a successful frame to rearm"); ui.end();
    UiInput cancelled = typed; cancelled.character_count = UINT32_MAX;
    ui.begin(cancelled); check(!grid(value).changed && std::string_view(value) == "A", "public text count overflow cancels before indexing"); ui.end();
    ui.begin(typed, {512, 288, 2, 0, 0}); check(!grid(value).changed, "view change cancels characters"); ui.end();

    framework::probe::allocs = 0; framework::probe::in_hot = true;
    const bool plain = framework::probe::control::plain_allocation(), aligned = framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(plain && aligned && framework::probe::allocs >= 2, "plain/aligned allocation counter positive controls");
    ui.begin({}); grid(value, "full", 8, {0, 0, 512, 288}, 16); ui.end();
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    for (uint32_t i = 0; i < 8; ++i) {
        ui.begin({}); grid(value, "full", 8, {0, 0, 512, 288}, 16); ui.end();
        ui.begin(erase); grid(value, "full", 8, {0, 0, 512, 288}, 16); ui.end();
    }
    framework::probe::in_hot = false;
    check(framework::probe::allocs == 0, "full grid frames allocate nothing");
    std::puts(fails ? "ui-grid-limits: FAIL" : "ui-grid-limits: PASS UTF8 bounds, capacities, refusal and zero allocations");
    return fails ? 1 : 0;
}
