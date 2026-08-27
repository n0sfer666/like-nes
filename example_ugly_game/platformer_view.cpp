#include "platformer_view.hpp"

#include "tile_shape.hpp"

namespace platformer {
namespace {

float f(fix32 v) { return static_cast<float>(v.to_double()); }

fix32 half(fix32 v) { return v / fix32::from_int(2); }

// Кламп по одной оси. Уровень уже вида (`hi < lo`) — прижимаемся к его началу: иначе камера
// уезжала бы за карту тем самым клампом, который её обязан держать.
fix32 clamp_axis(fix32 want, fix32 lo, fix32 hi) {
    if (hi.raw < lo.raw) return lo;
    if (want.raw < lo.raw) return lo;
    if (hi.raw < want.raw) return hi;
    return want;
}

// Диапазон по Y, который выпуклая форма занимает на вертикальной полосе [x0,x1] — в ЛОКАЛЬНЫХ осях
// формы. Считается по рёбрам, а не по формуле склона: формул было бы четыре (по зеркалу на каждое),
// и разошлись бы они с решателем при первой же правке `TileShapes::build`.
bool span(const ph::Shape& s, fix32 x0, fix32 x1, fix32& lo, fix32& hi) {
    bool any = false;
    auto add = [&](fix32 y) {
        if (!any) { lo = y; hi = y; any = true; return; }
        if (y.raw < lo.raw) lo = y;
        if (hi.raw < y.raw) hi = y;
    };
    for (uint8_t i = 0; i < s.count; ++i) {
        const Vec2 a = s.points[i];
        const Vec2 b = s.points[(i + 1) % s.count];
        if (x0.raw <= a.x.raw && a.x.raw <= x1.raw) add(a.y);
        const fix32 cut[2] = {x0, x1};
        for (const fix32 x : cut) {
            const bool crosses = (a.x.raw <= x.raw && x.raw <= b.x.raw) ||
                                 (b.x.raw <= x.raw && x.raw <= a.x.raw);
            if (!crosses) continue;
            if (a.x.raw == b.x.raw) { add(a.y); add(b.y); continue; }
            add(a.y + (b.y - a.y) * ((x - a.x) / (b.x - a.x)));
        }
    }
    return any;
}

// Мировая коробка → квад вида. Отбрасывание ЦЕЛИКОМ, без подрезки размеров: подрезанный квад врал
// бы гейту про геометрию тайла, а обрезать его по краю экрана и так умеет растеризатор.
void emit(std::vector<Quad>& out, const Camera& cam, fix32 left, fix32 top, fix32 w, fix32 h,
          Rgba color) {
    const fix32 x = left - cam.left;
    const fix32 y = top - cam.top;
    if ((x + w).raw <= 0 || (y + h).raw <= 0) return;
    if (fix32::from_int(VIEW_W).raw <= x.raw || fix32::from_int(VIEW_H).raw <= y.raw) return;
    out.push_back({f(x), f(y), f(w), f(h), color});
}

// Односторонняя площадка спрашивается ПЕРВОЙ: её бит живёт вместе с `solid` (`grid.hpp`), и порядок
// «сначала solid» покрасил бы её как стену — то есть скрыл бы единственное, чем она от стены
// отличается.
Rgba tile_color(tm::TileFlags fl) {
    if ((fl & tm::TILE_ONEWAY) != 0) return C_ONEWAY;
    if ((fl & tm::TILE_SLOPE) != 0) return C_SLOPE;
    return C_SOLID;
}

void push_tile(std::vector<Quad>& out, const Camera& cam, const ph::Aabb& box, tm::TileFlags fl,
               tm::TileShapes& shapes) {
    const Rgba color = tile_color(fl);
    const fix32 w = box.max.x - box.min.x;
    const fix32 h = box.max.y - box.min.y;
    if ((fl & tm::TILE_SLOPE) == 0) {
        emit(out, cam, box.min.x, box.min.y, w, h, color);
        return;
    }
    // Центр тайла, потому что форма склона задана вокруг него (`tile_shape.hpp`), и перенос в мир —
    // то же прибавление центра, которым её переносит запрос.
    const fix32 cx = box.min.x + half(w);
    const fix32 cy = box.min.y + half(h);
    const fix32 step = w / fix32::from_int(SLOPE_STEPS);
    for (int i = 0; i < SLOPE_STEPS; ++i) {
        const fix32 x0 = step * fix32::from_int(i) - half(w);
        fix32 lo, hi;
        if (!span(shapes.of(fl), x0, x0 + step, lo, hi)) continue;
        emit(out, cam, cx + x0, cy + lo, step, hi - lo, color);
    }
}

} // namespace

Camera camera_at(const Stage& st) {
    const fix32 want_x = st.hero.position.x - fix32::from_int(VIEW_W / 2);
    const fix32 want_y = st.hero.position.y - fix32::from_int(VIEW_H / 2);
    // Без сетки клампить не к чему: границ уровня не существует, и выдуманные здесь были бы
    // границами вида, а не карты. Живой путь сюда не доходит — `load_stage` без сетки не стартует.
    if (!st.grid) return {want_x, want_y};
    const Vec2 o = st.grid->origin();
    const fix32 ts = st.grid->tile_size();
    const fix32 right = o.x + ts * fix32::from_int(static_cast<int32_t>(st.grid->width()));
    const fix32 bottom = o.y + ts * fix32::from_int(static_cast<int32_t>(st.grid->height()));
    Camera c;
    c.left = clamp_axis(want_x, o.x, right - fix32::from_int(VIEW_W));
    c.top = clamp_axis(want_y, o.y, bottom - fix32::from_int(VIEW_H));
    return c;
}

void build_quads(const Stage& st, const Camera& cam, std::vector<Quad>& out) {
    out.clear();
    if (st.grid) {
        const tm::TileGrid& g = *st.grid;
        const Vec2 corner{cam.left + fix32::from_int(VIEW_W), cam.top + fix32::from_int(VIEW_H)};
        const tm::TileWindow win = g.window(ph::Aabb{{cam.left, cam.top}, corner});
        // Кеш форм живёт ровно один кадр — по тому же основанию, что и в запросе: форма зависит от
        // размера тайла, то есть от сетки, и общий на все сетки кеш пришлось бы сторожить ключом.
        tm::TileShapes shapes(half(g.tile_size()));
        for (int32_t ty = win.y0; ty < win.y1; ++ty)
            for (int32_t tx = win.x0; tx < win.x1; ++tx) {
                const tm::TileFlags fl = g.at(tx, ty);
                if (fl == tm::TILE_EMPTY) continue;
                push_tile(out, cam, g.tile_bounds(tx, ty), fl, shapes);
            }
    }
    // Платформа рисуется ДО персонажа, а персонаж последним: везомый стоит на её крыше, и обратный
    // порядок прятал бы его подошвы под плитой ровно в тот момент, ради которого она в уровне есть.
    const ph::Body& lift = st.world.body(st.lift);
    emit(out, cam, lift.position.x - LIFT_HALF_W, lift.position.y - LIFT_HALF_H,
         LIFT_HALF_W + LIFT_HALF_W, LIFT_HALF_H + LIFT_HALF_H, C_LIFT);
    emit(out, cam, st.hero.position.x - HULL_HALF_W, st.hero.position.y - HULL_HALF_H,
         HULL_HALF_W + HULL_HALF_W, HULL_HALF_H + HULL_HALF_H, C_HERO);
}

} // namespace platformer
