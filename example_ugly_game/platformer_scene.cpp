#include "platformer_scene.hpp"

#include <cstring>

#include "asset_manager.hpp"
#include "hash.hpp"

// Чтение уровня и кадр уровня. Бандл открывается ОДИН раз и закрывается здесь же: и сетка, и
// профиль КОПИРУЮТ прочитанное (`TileMapTable::build` копирует блок флагов, `ProfileTable::find` —
// поля), поэтому переживать mmap-регион им нечем и незачем. Держать менеджер открытым на весь
// прогон значило бы завести владение сроком в игру ради данных, прочитанных за одну загрузку.
namespace platformer {
namespace {

constexpr std::size_t ARENA_CAPACITY = 1024u * 1024u;

// Секция по имени. Имя хешируется тем же `fnv1a`, которым его писал пекарь: guid — это и есть имя,
// свёрнутое в число, и второй способ его посчитать означал бы второе определение формата.
bool section(asset::AssetManager& am, const char* name, const void*& data, std::size_t& size) {
    const uint64_t guid = asset::fnv1a(name, std::strlen(name));
    am.request(guid);
    am.sync_point();
    if (!am.is_ready(guid)) return false;
    const asset::Loaded a = am.get(guid);
    data = a.data;
    size = a.size;
    return true;
}

// Тайл в точке. Деление ЦЕЛОЧИСЛЕННОЕ по сырому Q16.16 и с округлением ВНИЗ — по тому же доводу,
// что в `TileGrid::window`: `fix32::operator/` усекает к нулю, и левее начала координат точка
// уезжала бы на тайл. Карта образца лежит в положительных координатах, но правило не про неё —
// про то, чтобы вопрос «какой тайл под ногами» не менял ответ от знака.
tm::TileFlags tile_at(const tm::TileGrid& g, Vec2 p) {
    const int64_t size = g.tile_size().raw;
    const int64_t dx = static_cast<int64_t>(p.x.raw) - g.origin().x.raw;
    const int64_t dy = static_cast<int64_t>(p.y.raw) - g.origin().y.raw;
    const int64_t tx = (dx >= 0 ? dx : dx - size + 1) / size;
    const int64_t ty = (dy >= 0 ? dy : dy - size + 1) / size;
    return g.at(static_cast<int32_t>(tx), static_cast<int32_t>(ty));
}

} // namespace

tm::TileFlags tile_at_point(const Stage& st, Vec2 p) {
    if (!st.grid) return tm::TILE_EMPTY;
    return tile_at(*st.grid, p);
}

bool load_stage(const std::string& bundle_path, Stage& out) {
    asset::AssetManager am;
    if (!am.open(bundle_path, ARENA_CAPACITY, /*trusted=*/false)) return false;

    const void* data = nullptr;
    std::size_t size = 0;
    bool ok = section(am, "tilemap", data, size);
    if (ok) {
        tm::TileMapTable maps;
        ok = maps.open(data, size);
        if (ok) {
            out.grid = maps.find("field");
            ok = out.grid.has_value();
        }
    }
    if (ok && section(am, "movement", data, size)) {
        ch::ProfileTable profiles;
        ok = profiles.open(data, size) && profiles.find("player", out.profile);
    } else {
        ok = false;
    }
    am.close();
    if (!ok) return false;

    out.derived = ch::derive(out.profile, tick_dt());

    // Мир без тяготения: единственное его тело кинематическое, а персонаж не тело решателя вовсе —
    // своё тяготение он берёт из профиля. Тяготение мира тут двигало бы платформу вниз, то есть
    // означало бы, что уровень собран не из того, из чего он собран.
    out.world.set_gravity({fix32{}, fix32{}});
    ph::BodyDesc d;
    d.key = 1;
    d.type = ph::BodyType::Kinematic;
    d.shape = ph::box(LIFT_HALF_W, LIFT_HALF_H);
    d.position = {LIFT_LEFT, LIFT_TOP + LIFT_HALF_H};
    d.velocity = {LIFT_SPEED, fix32{}};
    out.lift = out.world.add(d);

    // Появление на полу с тем же зазором, в котором персонаж и держится после первой пробы опоры.
    // Ровно на полу его ставить нельзя: свип нулевой длины из касания отвечает долей ноль, и первый
    // же тик читался бы как «упёрся», а не как «стоит».
    out.hero = ch::Character{};
    out.hero.position = {SPAWN_X, FLOOR_TOP - HULL_HALF_H - ch::SKIN};
    out.hero.on_ground = true;
    out.hero.state = ch::MoveState::Ground;
    return true;
}

void step_stage(Stage& st, const ch::MoveInput& in) {
    // Разворот платформы — до шага мира, а не после: скорость, выставленная после интегрирования,
    // применилась бы только на следующем кадре, и платформа уезжала бы за свой маршрут на тик.
    // Ручка `mutate` действительна до ближайшего запроса или шага, поэтому берётся вплотную к нему.
    const fix32 x = st.world.body(st.lift).position.x;
    const fix32 v = st.world.body(st.lift).velocity.x;
    if ((x.raw >= LIFT_RIGHT.raw && v.raw > 0) || (x.raw <= LIFT_LEFT.raw && v.raw < 0))
        st.world.mutate(st.lift).velocity.x = fix32{} - v;

    st.world.step(tick_dt());
    ch::step(st.view(), st.hull(), st.profile, st.derived, in, tick_dt(), st.hero);
}

tm::TileFlags ground_tile(const Stage& st) {
    if (!st.hero.on_ground || !st.grid) return tm::TILE_EMPTY;
    // Точка пробы — на четверть тайла ниже подошвы: зазор `SKIN` мельче её, поэтому она попадает в
    // тайл под ногами и на плоском полу, и на склоне, где подошва стоит выше самой грани.
    const Vec2 under = {st.hero.position.x,
                        st.hero.position.y + HULL_HALF_H + fix32::from_int(4)};
    return tile_at(*st.grid, under);
}

} // namespace platformer
