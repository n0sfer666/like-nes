#include <cstring>

#include "asset_manager.hpp"
#include "hash.hpp"
#include "platformer_scene.hpp"

// ЧТЕНИЕ уровня: бандл разворачивается в сетку, профиль и одно тело, а персонаж встаёт в точку
// появления. Отдельным файлом от кадра уровня (`platformer_scene.cpp`) по правилу единственной
// ответственности: здесь всё, что случается ОДИН раз на загрузку, там — то, что случается каждый
// кадр, и связывает их ровно точка появления, объявленная в заголовке.
//
// Бандл открывается один раз и закрывается здесь же: и сетка, и профиль КОПИРУЮТ прочитанное
// (`TileMapTable::build` копирует блок флагов, `ProfileTable::find` — поля), поэтому переживать
// mmap-регион им нечем и незачем. Держать менеджер открытым на весь прогон значило бы завести
// владение сроком в игру ради данных, прочитанных за одну загрузку.
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

} // namespace

// Точка появления: на полу с тем же зазором, в котором персонаж и держится после первой пробы
// опоры. Ровно на полу его ставить нельзя — свип нулевой длины из касания отвечает долей ноль, и
// первый же тик читался бы как «упёрся», а не как «стоит». Одним местом на загрузку и на возврат
// раздавленного: две копии этих четырёх строк разъехались бы на первой же правке карты.
void place_at_spawn(Stage& st) {
    st.hero = ch::Character{};
    st.hero.position = {SPAWN_X, FLOOR_TOP - HULL_HALF_H - ch::SKIN};
    st.hero.on_ground = true;
    st.hero.state = ch::MoveState::Ground;
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

    place_at_spawn(out);
    return true;
}

} // namespace platformer
