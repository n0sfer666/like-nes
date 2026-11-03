#include "ui_context_fixture.hpp"

#include <bit>
#include <cstdio>
#include <limits>

#include "hash.hpp"

namespace {
using namespace framework::ui;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("ui-context: FAIL %s\n", why); } }
UiInput tap(float x, float y, intptr_t id = INTPTR_MAX) {
    UiInput in; in.pointer_count = 2;
    in.pointers[0] = {id, UiPointerPhase::Down, x, y}; in.pointers[1] = {id, UiPointerPhase::Up, x, y};
    return in;
}
uint64_t hash(const UiOutput& out) {
    auto h = asset::fnv1a_u64(asset::FNV_OFFSET, out.focus.value_or(0));
    for (const auto& q : out.quads) {
        for (float v : {q.x, q.y, q.w, q.h, q.tx, q.ty, q.tw, q.th}) h = asset::fnv1a_u32(h, std::bit_cast<uint32_t>(v));
        h = asset::fnv1a_u32(h, q.rgba); h = asset::fnv1a_u32(h, q.flip);
    }
    for (const auto& run : out.runs) { h = asset::fnv1a_u32(h, run.first); h = asset::fnv1a_u32(h, run.count); h = asset::fnv1a_u32(h, run.texture); }
    return h;
}
}
int main() {
    ui_context_fixture::Scene scene;
    check(scene.init(), "baked strings/font linked by font_name");
    auto& ui = scene.context;
    const UiId a = ui_id(0, "item", 0), b = ui_id(0, "item", 1), slider = ui_id(0, "slider"), field = ui_id(0, "field");
    check(a != b && a != ui_id(1, "item", 0), "IDs include parent, key and index");
    UiInput early; early.accept = true;
    ui.begin(early); check(!ui.button(a, "one", {0, 0, 40, 20}), "unseen button ignores old keyboard activation"); ui.end();
    check(ui.init(scene.strings, scene.fonts, 3, 7), "reset before frame oracle");
    check(ui.begin(tap(10, 10)), "first frame begins");
    check(!ui.button(a, "one", {0, 0, 40, 20}) && !ui.button(b, "two", {0, 22, 40, 20}), "unseen first frame cannot receive a tap");
    auto out = ui.end();
    check(out.focus == a && out.stats.widgets == 2 && out.quads.size() == 7 && out.runs.size() == 4, "first focus, font quads and merged solid runs");
    check(out.quads[0].x == 0 && out.quads[0].w == 40 && out.quads[2].x == 3 && out.quads[2].y == 3 &&
          out.quads[2].tx == 4 && out.quads[2].tw == 3 && out.quads[2].th == 8, "independent glyph geometry oracle");
    const uint64_t golden = hash(out);
    check(golden == 0x2a01d5ac8d6063b0ull && a == 0x9dd49e545e8d6500ull, "independent Python struct/FNV quad-focus oracle");
    std::printf("ui-context: quad-focus hash=%016llx\n", static_cast<unsigned long long>(golden));

    UiInput down; down.y = 1; down.accept = true;
    ui.begin(down);
    check(!ui.button(a, "one", {0, 0, 40, 20}) && ui.button(b, "two", {0, 22, 40, 20}), "keyboard navigation and activation");
    out = ui.end(); check(out.focus == b, "second item keeps distinct focus");
    ui.begin(tap(10, 10));
    check(ui.button(a, "one", {0, 0, 40, 20}), "real tap focuses and activates last visible target");
    ui.button(b, "two", {0, 22, 40, 20}); out = ui.end(); check(out.focus == a, "touch focus follows hit");
    ui.begin(tap(80, 80)); check(!ui.button(a, "one", {0, 0, 40, 20}), "outside tap ignored"); ui.end();
    ui.begin(down); check(!ui.button(b, "two", {0, 22, 40, 20}), "new identity does not receive old screen activation");
    out = ui.end(); check(out.focus == b && out.quads.front().rgba == UiTheme{}.focused, "removed focus repaired and painted in same frame");
    ui.begin({}); ui.button(b, "two", {0, 22, 40, 20}, false); ui.button(a, "one", {0, 0, 40, 20});
    out = ui.end(); check(out.focus == a, "disabled current focus repaired");
    ui.set_language(Language::Ru); ui.begin({}); ui.button(a, "one", {0, 0, 40, 20}); out = ui.end();
    check(out.quads[2].tx == 8 && out.quads[2].tw == 5, "selected language uses covered Cyrillic glyph");

    check(ui.init(scene.strings, scene.fonts, 3, 7), "fresh context for slider visibility guard");
    float value = .5f;
    UiInput right; right.x = 1;
    ui.begin(right); check(!ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f) && value == .5f,
                          "unseen slider ignores old keyboard direction"); ui.end();
    ui.begin(right); check(ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f) && value == .75f, "focused slider steps and reports change"); ui.end();
    ui.begin(tap(500, 10)); ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f); ui.end();
    UiInput press; press.pointer_count = 1; press.pointers[0] = {5, UiPointerPhase::Down, 50, 10};
    ui.begin(press); ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f); ui.end();
    UiInput move; move.pointer_count = 1; move.pointers[0] = {5, UiPointerPhase::Up, 1000, 10};
    ui.begin(move); check(ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f) && value == 1, "captured slider drag clamps outside"); ui.end();
    value = std::numeric_limits<float>::quiet_NaN();
    ui.begin(right); check(!ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f), "invalid slider value refused");
    out = ui.end(); check(out.stats.invalid_widgets == 1 && !out.focus, "bad slider disabled");
    value = 2;
    ui.begin({}); check(!ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f) && value == 2,
                       "drawing clamps the bar without changing caller data"); ui.end();
    UiInput cancelled_range; cancelled_range.cancelled = true; cancelled_range.x = -1;
    ui.begin(cancelled_range); check(!ui.slider(slider, "volume", {0, 0, 100, 24}, value, 0, 1, .25f) && value == 2,
                                  "cancelled slider does not normalize persisted data"); ui.end();
    char name[] = "A";
    ui.begin({}); ui.field(field, "name", {0, 0, 80, 20}, name); ui.end();
    ui.begin(tap(10, 10)); check(ui.field(field, "name", {0, 0, 80, 20}, name), "field activates without modifying value"); ui.end();
    check(std::string_view(name) == "A", "field borrows user buffer only during draw");
    const UiListItem items[]{{"row", "one"}, {"row", "two"}};
    ui.begin({}); ui.list(0, items, {0, 0, 40, 40}, 20); ui.end();
    ui.begin(down); const auto selected = ui.list(0, items, {0, 0, 40, 40}, 20); out = ui.end();
    check(selected == 1 && out.stats.duplicate_ids == 0, "list index prevents identical keys sharing focus");
    UiInput cancelled = tap(10, 10); cancelled.cancelled = true;
    ui.begin(cancelled); check(!ui.list(0, items, {0, 0, 40, 40}, 20), "cancel suppresses activation"); ui.end();
    ui.begin(tap(10, 10), {512, 288, 2, 7, 9}); check(!ui.list(0, items, {0, 0, 40, 40}, 20), "view change cancels old pointer geometry");
    out = ui.end(); check(out.quads[0].x == 7 && out.quads[0].w == 80 && out.quads[2].x == 13, "zone geometry becomes screen pixels times k");
    check(!ui.begin({}, {0, 288, 1, 0, 0}) && ui.end().quads.empty(), "zero view refused without stale frame");
    check(ui.begin({}) && !ui.begin({}) && ui.end().quads.empty(), "nested begin fails closed and recovers next frame");
    check(ui.begin({}), "valid begin after misuse"); ui.end();
    std::puts(fails ? "ui-context: FAIL" : "ui-context: PASS focus, IDs, widgets, pointer capture, language and quads");
    return fails ? 1 : 0;
}
