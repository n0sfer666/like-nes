#include "platformer_view.hpp"

#include "tile_shape.hpp"

namespace platformer {
namespace {

float f(fix32 v) { return static_cast<float>(v.to_double()); }

fix32 half(fix32 v) { return v / fix32::from_int(2); }

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

// Односторонняя площадка спрашивается ПЕРВОЙ: её бит живёт вместе с `solid` (`grid.hpp`), и порядок
// «сначала solid» покрасил бы её как стену — то есть скрыл бы единственное, чем она от стены
// отличается.
uint32_t kind_tint(tm::TileFlags fl) {
    if ((fl & tm::TILE_ONEWAY) != 0) return C_ONEWAY;
    if ((fl & tm::TILE_SLOPE) != 0) return C_SLOPE;
    return C_SOLID;
}

// Набор перечисляет КОМБИНАЦИИ битов, а не «типы тайлов»: индекс таблицы у `draw_tiles` — сами
// флаги, поэтому площадка (`solid|oneway`) и стена (`solid`) это два разных класса, и заполнять
// таблицу приходится обходом всех шести разрядов.
//
// Склон из набора ВЫЧЕРКНУТ (регион нулевой): целым квадом холм выглядел бы стеной, и персонаж —
// идущим сквозь неё. Его рисует второй проход ступенькой подквадов.
gx::TileSet stage_tiles() {
    gx::TileSet set;
    set.rgba = C_SOLID;
    set.layer = L_TILES;
    for (uint32_t k = 1; k < gx::TILE_KINDS; ++k) {
        const tm::TileFlags fl = static_cast<tm::TileFlags>(k);
        if ((fl & tm::TILE_SLOPE) != 0) continue;
        set.region[k] = R_SOLID;
        set.tint[k] = kind_tint(fl);
    }
    return set;
}

// Отбрасывание ЦЕЛИКОМ, без подрезки размеров: подрезанный квад врал бы гейту про геометрию тайла,
// а обрезать его по краю экрана и так умеет растеризатор. Сравнения строгие — коробка, касающаяся
// края вида ровно нулевой площадью, невидима.
bool visible(const ph::Aabb& view, Vec2 center, Vec2 half) {
    return view.min.x.raw < (center.x + half.x).raw && (center.x - half.x).raw < view.max.x.raw &&
           view.min.y.raw < (center.y + half.y).raw && (center.y - half.y).raw < view.max.y.raw;
}

void push_box(gx::SpriteList& list, const ph::Aabb& view, Vec2 center, Vec2 half, uint32_t color,
              int16_t layer) {
    if (!visible(view, center, half)) return;
    gx::Sprite s;
    s.center = center;
    s.half = half;
    s.rgba = color;
    s.region = R_SOLID;
    s.layer = layer;
    list.push(s);
}

// Склоны окна — вторым проходом по тем же тайлам. Это ВТОРОЙ обход окна, и он осознан: сложить оба
// в один значило бы просить `draw_tiles` знать про формы тайлов, то есть тащить в отрисовку
// решатель физики. Окно здесь — 22×17 тайлов при любом размере карты, поэтому цена обхода не растёт
// с уровнем; тем же числом её меряет гейт 7 спеки #17.
void push_slopes(gx::SpriteList& list, const tm::TileGrid& g, const ph::Aabb& view) {
    const tm::TileWindow win = g.window(view);
    // Кеш форм живёт ровно один кадр — по тому же основанию, что и в запросе: форма зависит от
    // размера тайла, то есть от сетки, и общий на все сетки кеш пришлось бы сторожить ключом.
    tm::TileShapes shapes(half(g.tile_size()));
    for (int32_t ty = win.y0; ty < win.y1; ++ty)
        for (int32_t tx = win.x0; tx < win.x1; ++tx) {
            const tm::TileFlags fl = g.at(tx, ty);
            if ((fl & tm::TILE_SLOPE) == 0) continue;
            const ph::Aabb box = g.tile_bounds(tx, ty);
            const fix32 w = box.max.x - box.min.x;
            // Центр тайла, потому что форма склона задана вокруг него (`tile_shape.hpp`), и перенос
            // в мир — то же прибавление центра, которым её переносит запрос.
            const Vec2 c{box.min.x + half(w), box.min.y + half(box.max.y - box.min.y)};
            const fix32 step = w / fix32::from_int(SLOPE_STEPS);
            for (int i = 0; i < SLOPE_STEPS; ++i) {
                const fix32 x0 = step * fix32::from_int(i) - half(w);
                fix32 lo, hi;
                if (!span(shapes.of(fl), x0, x0 + step, lo, hi)) continue;
                push_box(list, view, {c.x + x0 + half(step), c.y + lo + half(hi - lo)},
                         {half(step), half(hi - lo)}, C_SLOPE, L_TILES);
            }
        }
}

} // namespace

gx::Viewport stage_viewport() {
    gx::Viewport v;
    v.screen_half = {fix32::from_int(VIEW_W / 2), fix32::from_int(VIEW_H / 2)};
    v.zoom = fix32::from_int(1);
    v.pixels_per_unit = 1;
    return v;
}

Vec2 view_origin(const Camera& cam) { return cam.center - gx::viewport_half_world(stage_viewport()); }

Camera camera_at(const Stage& st) {
    gx::CameraConfig cfg;
    cfg.half_view = gx::viewport_half_world(stage_viewport());
    // Без сетки клампить не к чему: границ уровня не существует, и выдуманные здесь были бы
    // границами вида, а не карты. Живой путь сюда не доходит — `load_stage` без сетки не стартует.
    if (st.grid) {
        const Vec2 o = st.grid->origin();
        const fix32 ts = st.grid->tile_size();
        cfg.policies = gx::CAMERA_BOUNDS;
        cfg.bounds = {o.x, o.y,
                      o.x + ts * fix32::from_int(static_cast<int32_t>(st.grid->width())),
                      o.y + ts * fix32::from_int(static_cast<int32_t>(st.grid->height()))};
    }
    // Камера каждый кадр считается ЗАНОВО, поэтому следование берётся с нуля: ни скорости, ни
    // мёртвой зоны у образца не было и раньше, а завести их здесь значило бы менять поведение под
    // видом перевода на фреймворк.
    Camera c;
    gx::camera_follow(c, cfg, st.hero.position, 0);
    return c;
}

// Верхняя граница кадра, а не догадка: окно вида это 22×17 тайлов при тайле в 16, и худший тайл —
// склон, дающий `SLOPE_STEPS` подквадов. Плюс платформа и персонаж. Список, которому хватает по
// построению, не может потерять квад молча — а потерянный выглядел бы ровно как «так и задумано».
FrameSprites::FrameSprites()
    : sprites_(static_cast<size_t>((VIEW_W / 16 + 2) * (VIEW_H / 16 + 2) * SLOPE_STEPS + 2)),
      keys_(sprites_.size()) {}

gx::SpriteList FrameSprites::list() {
    return gx::SpriteList(sprites_.data(), keys_.data(), static_cast<uint32_t>(sprites_.size()));
}

void build_quads(const Stage& st, const Camera& cam, FrameSprites& buf, std::vector<Quad>& out) {
    out.clear();
    const gx::Viewport view = stage_viewport();
    const Vec2 origin = view_origin(cam);
    // Дальний угол вида — ПОСЛЕДНЯЯ ВИДИМАЯ точка, а не первая невидимая. Окно тайлов включает
    // правый край (`grid.cpp`), потому что зонд, поставленный впритык к стене, обязан стену найти;
    // отрисовке же столбец, начинающийся ровно на краю экрана, не виден ни одним пикселем, и
    // поданный он был бы занятым слотом батча под квадом за экраном.
    const fix32 last = fix32::from_raw(1);
    const Vec2 far = origin + gx::viewport_half_world(view) * fix32::from_int(2);
    const ph::Aabb box{origin, {far.x - last, far.y - last}};

    gx::SpriteList list = buf.list();
    if (st.grid) {
        draw_tiles(list, *st.grid, box, stage_tiles());
        push_slopes(list, *st.grid, box);
    }
    const ph::Body& lift = st.world.body(st.lift);
    push_box(list, box, lift.position, {LIFT_HALF_W, LIFT_HALF_H}, C_LIFT, L_LIFT);
    push_box(list, box, st.hero.position, {HULL_HALF_W, HULL_HALF_H}, C_HERO, L_HERO);

    // Батч здесь ровно один — материал у образца общий, — но `build` зовётся не ради их числа: он
    // сортирует ключи, то есть задаёт порядок, в котором ниже читается `drawn`.
    gx::Batch batches[4];
    list.build(batches, 4);

    const fix32 s = gx::viewport_scale(view);
    for (uint32_t i = 0; i < list.count(); ++i) {
        const gx::Sprite& sp = list.drawn(i);
        const Vec2 p = gx::world_to_screen(view, cam.center, sp.center);
        const fix32 w = sp.half.x * s;
        const fix32 h = sp.half.y * s;
        out.push_back({f(p.x - w), f(p.y - h), f(w + w), f(h + h), sp.rgba});
    }
}

} // namespace platformer
