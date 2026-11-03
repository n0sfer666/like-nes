#include "ui_context_fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

namespace {
using namespace framework::ui;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("ui-context-limits: FAIL %s\n", why); } }
UiInput tap(float x, float y) {
    UiInput input; input.pointer_count = 2;
    input.pointers[0] = {7, UiPointerPhase::Down, x, y}; input.pointers[1] = {7, UiPointerPhase::Up, x, y}; return input;
}
}
int main() {
    ui_context_fixture::Scene scene;
    check(scene.init(true), "large phrase, actual baker and readers");
    auto& ui = scene.context;
    check(!ui.button(0, "one", {0, 0, 20, 20}) && ui.end().quads.empty(), "widgets outside a frame do nothing");
    ui.begin({}); ui.button(0, "one", {0, 0, 20, 20}); auto out = ui.end();
    check(out.focus.has_value() && *out.focus == 0, "full ID range includes zero");
    ui.begin({}); ui.button(0, "one", {INT32_MAX, 0, 20, 20}); out = ui.end();
    check(out.stats.invalid_widgets == 1 && out.quads.empty() && !out.focus, "invalid rectangle refused before coordinate arithmetic");
    ui.begin({}); ui.button(0, "absent", {0, 0, 20, 20}); out = ui.end();
    check(out.stats.missing_labels == 1 && !out.focus, "missing label disables unnamed control");
    ui.begin({}); ui.button(0, "one", {0, 0, 20, 20}); ui.button(0, "two", {0, 20, 20, 20}); out = ui.end();
    check(out.stats.widgets == 1 && out.stats.duplicate_ids == 1, "duplicate IDs cannot share activation");
    std::array<char, UI_FIELD_BYTES + 1> too_long; too_long.fill('A'); too_long.back() = 0;
    ui.begin({}); ui.field(0, "name", {0, 0, 80, 20}, too_long); out = ui.end();
    check(out.stats.invalid_widgets == 1 && !out.focus, "field termination checked with a bounded scan");
    UiInput invalid; invalid.accept = true; invalid.pointer_count = UINT32_MAX;
    ui.begin(invalid); check(!ui.button(0, "one", {0, 0, 20, 20}), "malformed public UiInput cannot bypass count caps"); ui.end();
    invalid = tap(1, 1); invalid.pointers[0].x = std::numeric_limits<float>::infinity();
    ui.begin(invalid); check(!ui.button(0, "one", {0, 0, 20, 20}), "malformed public coordinate cancels"); ui.end();
    invalid = {}; invalid.accept = true;
    ui.begin(invalid); check(!ui.button(0, "one", {600, 0, 20, 20}), "fully clipped widget is not keyboard active");
    out = ui.end(); check(!out.focus && out.quads.empty(), "fully clipped widget cannot retain focus");
    ui.begin({}); ui.button(1, "one", {0, 0, 20, 20}); ui.button(2, "two", {0, 0, 20, 20}, false); ui.end();
    ui.begin(tap(1, 1)); check(!ui.button(1, "one", {0, 0, 20, 20}), "disabled painted overlay blocks click-through");
    ui.button(2, "two", {0, 0, 20, 20}, false); ui.end();
    ui.begin({}); ui.button(0, "one", {-10, 0, 20, 20}); ui.end();
    auto outside_up = tap(1, 1); outside_up.pointers[1].x = -5;
    ui.begin(outside_up); check(!ui.button(0, "one", {-10, 0, 20, 20}), "release outside zone cannot activate hidden part of a widget"); ui.end();
    auto cancel_after_up = tap(1, 1); cancel_after_up.pointer_count = 3;
    cancel_after_up.pointers[2] = {7, UiPointerPhase::Cancel, 0, 0};
    ui.begin(cancel_after_up); check(!ui.button(0, "one", {-10, 0, 20, 20}), "cancel also discards a queued release click"); ui.end();
    ui.begin({}, {512, 288, 2, 7, 9}); ui.button(0, "one", {-2, -4, 20, 20}); out = ui.end();
    check(out.quads.size() == 3 && out.quads[2].x == 9 && out.quads[2].y == 9 && out.quads[2].w == 6 &&
          out.quads[2].h == 14 && out.quads[2].tx == 4 && out.quads[2].ty == 1 && out.quads[2].th == 7,
          "clipping adjusts glyph UV and destination before viewport scale");

    const UiView large_view{512, 32767, 1, 0, 0};
    auto fill_capacity = [&](const UiInput& input) {
        ui.begin(input, large_view);
        for (uint32_t i = 0; i < UI_WIDGETS; ++i) ui.button(ui_id(0, "long-row", i), "long", {0, static_cast<int32_t>(i * 64), 32, 64});
        return ui.end();
    };
    out = fill_capacity({});
    check(out.stats.widgets == UI_WIDGETS && out.quads.size() == UI_QUADS && out.stats.dropped_quads == 4352 &&
          out.stats.clipped_glyphs == UI_WIDGETS * 215, "bounded 256-glyph prefix and fixed quad capacity have independent exact counts");
    uint32_t covered = 0;
    for (const auto& run : out.runs) { check(run.first == covered && run.count > 0, "runs partition the output"); covered += run.count; }
    check(covered == out.quads.size(), "every emitted quad belongs to exactly one run");
    for (uint32_t hit : {40u, 127u}) {
        ui.begin(tap(1, static_cast<float>(hit * 64 + 1)), large_view);
        for (uint32_t i = 0; i < UI_WIDGETS; ++i) {
            const bool fired = ui.button(ui_id(0, "long-row", i), "long", {0, static_cast<int32_t>(i * 64), 32, 64});
            check(!fired, "partial or unpainted capacity-dropped widget cannot accept pointer input");
        }
        ui.end();
    }
    float saved = .5f;
    ui.begin({}); ui.slider(5000, "volume", {100, 0, 80, 24}, saved, 0, 1, .25f); ui.end();
    UiInput change; change.x = 1;
    ui.begin(change);
    for (uint32_t i = 0; i < 41; ++i) ui.button(i, "long", {0, 0, 32, 64});
    check(!ui.slider(5000, "volume", {100, 0, 80, 24}, saved, 0, 1, .25f) && saved == .5f,
          "slider changes roll back when its frame cannot be drawn"); ui.end();
    ui.begin({});
    for (uint32_t i = 0; i <= UI_WIDGETS; ++i) ui.button(i, "one", {0, 0, 20, 20});
    out = ui.end(); check(out.stats.widgets == UI_WIDGETS && out.stats.dropped_widgets == 1, "fixed widget capacity");
    const UiListItem items[]{{"bad key", "one"}, {"ok", "two"}};
    ui.begin({}); ui.list(0, items, {0, 0, 40, 40}, 20); out = ui.end();
    check(out.stats.invalid_widgets == 1 && out.stats.widgets == 1, "list key validation bounds per-frame ID hashing");

    framework::probe::in_hot = true;
    const bool control = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(control && framework::probe::allocs >= 2, "allocation positive control");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    out = fill_capacity({});
    char name[] = "A"; float volume = .5f;
    for (int frame = 0; frame < 8; ++frame) {
        ui.begin({}); ui.button(0, "one", {0, 0, 50, 20}); ui.slider(1, "volume", {0, 22, 80, 24}, volume, 0, 1, .25f);
        ui.field(2, "name", {0, 48, 80, 20}, name); ui.list(3, items, {0, 70, 40, 40}, 20); out = ui.end();
    }
    framework::probe::in_hot = false;
    check(framework::probe::allocs == 0, "all widget methods, clipping and overflow allocate nothing");
    framework::graphics::FontTable absent;
    check(!ui.init(scene.strings, absent, 3, 7) && !ui.begin({}) && ui.end().quads.empty(), "failed reinit discards old frame/state");
    std::puts(fails ? "ui-context-limits: FAIL" : "ui-context-limits: PASS bounds, clipping, capacities, cancellation and zero allocations");
    return fails ? 1 : 0;
}
