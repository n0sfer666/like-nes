#include "../scene.hpp"
#include "gizmo.hpp"
#include "property_grid.hpp"
#include <cmath>
#include <cstdio>
#include <string>

// Гейт 6 headless data-путь (спека #7): property-grid из flecs meta (генерик, любой компонент) +
// гизмо/камера transform (детерм. round-trip). Live-окно (вьюпорт+инспектор+гизмо на экране) —
// owner-HW (editor_shell). Этот гейт доказывает DATA, что кормит UI.
using namespace ide;
using namespace ide::editor;

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }

const PropRow* find(const std::vector<PropRow>& rows, const char* comp, const char* mem) {
    for (const auto& r : rows)
        if (r.component == comp && r.member == mem) return &r;
    return nullptr;
}
} // namespace

int main() {
    // --- Property-grid из meta (генерик) ---
    Scene s;
    auto e = s.create(42);
    e.set<Name>({"hero"});
    e.set<Position>({fix32::from_int(3), fix32::from_int(5)});
    e.set<Velocity>({fix32::from_raw(65536), fix32()});
    e.set<Parent>({7});

    auto rows = build_property_grid(s, 42);
    check(!rows.empty(), "property-grid produced rows from meta");

    const PropRow* px = find(rows, "Position", "x");
    check(px != nullptr, "Position.x row present (meta-driven)");
    check(px && px->kind == "fix32", "Position.x kind = fix32 (opaque resolved)");
    check(px && px->value.rfind("196608", 0) == 0, "Position.x raw value = 196608 (3.0 fix32)");

    const PropRow* vx = find(rows, "Velocity", "x");
    check(vx && vx->value.rfind("65536", 0) == 0, "Velocity.x raw = 65536 (1.0 fix32)");

    const PropRow* nm = find(rows, "Name", "value");
    check(nm != nullptr, "Name.value row present");
    check(nm && nm->kind == "string" && nm->value == "hero", "Name.value = string 'hero'");

    const PropRow* pg = find(rows, "Parent", "guid");
    check(pg != nullptr && pg->value == "7", "Parent.guid = u64 7");

    // Пустая сущность → пустой grid (не крэш)
    s.create(99);
    check(build_property_grid(s, 99).empty(), "empty entity -> empty grid");
    check(build_property_grid(s, 12345).empty(), "missing guid -> empty grid");

    // --- Гизмо / камера transform (детерм. round-trip) ---
    Camera2D cam;
    cam.px = 10; cam.py = -5; cam.zoom = 2.0f; cam.vw = 800; cam.vh = 600;
    for (float wx : {-100.f, 0.f, 37.5f}) {
        for (float wy : {-50.f, 0.f, 88.25f}) {
            float sx, sy, rx, ry;
            world_to_screen(cam, wx, wy, sx, sy);
            screen_to_world(cam, sx, sy, rx, ry);
            check(std::fabs(rx - wx) < 1e-3f && std::fabs(ry - wy) < 1e-3f,
                  "world->screen->world round-trip identity");
        }
    }
    // Центр камеры (px,py) → центр экрана
    {
        float sx, sy;
        world_to_screen(cam, cam.px, cam.py, sx, sy);
        check(std::fabs(sx - cam.vw * 0.5f) < 1e-3f && std::fabs(sy - cam.vh * 0.5f) < 1e-3f,
              "camera center maps to screen center");
    }

    // --- Гизмо хит-тест ---
    Gizmo g;
    g.sx = 400; g.sy = 300; g.len = 60; g.grab = 6;
    check(gizmo_hit(g, 400, 300) == GizmoPart::Center, "hit center");
    check(gizmo_hit(g, 440, 300) == GizmoPart::AxisX, "hit X axis");
    check(gizmo_hit(g, 400, 260) == GizmoPart::AxisY, "hit Y axis");
    check(gizmo_hit(g, 200, 200) == GizmoPart::None, "miss -> None");

    bool pass = (failures == 0);
    std::printf("ide-editor-data: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
