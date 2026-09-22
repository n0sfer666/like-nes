// Цели гейта 9 из framework'а: пресеты ввода, тайловая карта, атлас и профиль движения. Четыре
// zero-parse читателя секций — те самые, чьё выравнивание закрывала A·2·6; здесь предмет другой,
// содержимое после успешного `open()`.
#include <cstring>
#include <optional>
#include <vector>

#include "framework/character/profile_bake.hpp"
#include "framework/character/profile_read.hpp"
#include "framework/graphics/atlas_bake.hpp"
#include "framework/graphics/atlas_read.hpp"
#include "framework/input/preset_bake.hpp"
#include "framework/input/presets.hpp"
#include "framework/tilemap/map_bake.hpp"
#include "framework/tilemap/map_read.hpp"
#include "fuzz_target.hpp"

namespace fuzz {
namespace {

namespace in = framework::input;
namespace tl = framework::tilemap;
namespace gr = framework::graphics;
namespace ch = framework::character;

// Исходники — те же наименьшие законные манифесты, что у гейта выравнивания секций. Мелкое семя
// здесь ПРЕИМУЩЕСТВО: мутатор ходит по всей длине, и байты заголовка получают свою долю правок,
// а не тонут в тысяче байт содержимого.
const char* const PRESET_SRC = "preset | p\naxis | move_x | key:d | key:a\n";
const char* const MAP_SRC =
    "map | field\ntile_size | 16\norigin | 0 | -32\nlegend | . | empty\nlegend | X | solid\n"
    "row | .X.\nrow | XXX\n";
const char* const ATLAS_SRC = "atlas | 64 | 32\nregion | first | 0 | 0 | 16 | 8 | 8 | 4\n";
const char* const PROFILE_SRC =
    "profile | player\nmax_speed | 340\nground_accel | 2400\nground_decel | 3200\n"
    "air_accel | 1600\nair_decel | 900\ngravity_rise | 1200\ngravity_fall | 2400\n"
    "max_fall_speed | 900\njump_height | 64\nmin_jump_height | 16\ncoyote_ticks | 6\n"
    "buffer_ticks | 6\ncorner_correction | 4\nground_snap | 8\nmax_slope | 1\nclimb_speed | 120\n"
    "ladder_regrab_ticks | 8\n";

// --- пресеты ввода (LNFI) ---

std::vector<uint8_t> seed_presets() {
    std::vector<uint8_t> bytes;
    in::PresetBakeError err;
    in::bake_presets(PRESET_SRC, bytes, err);
    return bytes;
}

bool read_presets(const uint8_t* data, size_t size) {
    in::PresetTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t p = 0; p < t.preset_count(); ++p) {
        const char* pname = t.preset_name(p);
        consume_str(pname);
        consume(static_cast<uint64_t>(t.find_preset(pname)));
        for (uint32_t a = 0; a < t.action_count(p); ++a) {
            const char* aname = t.action_name(p, a);
            consume_str(aname);
            consume(static_cast<uint64_t>(t.find_action(p, aname)));
            for (uint32_t b = 0; b < t.action_binding_count(p, a); ++b) {
                ::input::Source src{};
                // Поля источника, а не `sizeof`: размер структуры — константа компилятора, она
                // одинакова на валидном и на битом входе, и сток с ней не обходит НИЧЕГО из того,
                // что читатель достал из файла.
                if (t.action_source(p, a, b, src)) {
                    consume_all(static_cast<uint64_t>(src.kind), src.code, src.sign);
                }
            }
        }
        for (uint32_t x = 0; x < t.axis_count(p); ++x) {
            const in::StickShape sh = t.axis_shape(p, x);
            consume_all(sh.deadzone.raw, sh.outer.raw, sh.curve_exp);
            consume(t.axis_pair(p, x));
        }
    }
    return true;
}

// --- тайловая карта (LNTM) ---

std::vector<uint8_t> seed_maps() {
    std::vector<uint8_t> bytes;
    tl::MapBakeError err;
    tl::bake_maps(MAP_SRC, bytes, err);
    return bytes;
}

bool read_maps(const uint8_t* data, size_t size) {
    tl::TileMapTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        consume_str(t.name(i));
        // `build()` разворачивает строки карты в сетку — то есть ЧИТАЕТ массив флагов, о длине
        // которого врёт заголовок. Без него мутация по width/height трогала бы только число.
        const std::optional<tl::TileGrid> grid = t.build(i);
        if (!grid) continue;
        for (uint32_t y = 0; y < grid->height(); ++y) {
            for (uint32_t x = 0; x < grid->width(); ++x) {
                consume(grid->at(static_cast<int32_t>(x), static_cast<int32_t>(y)));
            }
        }
    }
    return true;
}

// --- атлас (LNAR) ---

std::vector<uint8_t> seed_atlas() {
    std::vector<uint8_t> bytes;
    gr::AtlasBakeError err;
    gr::bake_atlas(ATLAS_SRC, bytes, err);
    return bytes;
}

bool read_atlas(const uint8_t* data, size_t size) {
    gr::AtlasTable t;
    if (!t.open(data, size)) return false;
    consume_all(t.page_width(), t.page_height());
    for (uint16_t i = 0; i < t.count(); ++i) {
        const gr::RegionId id = static_cast<gr::RegionId>(i);
        const char* rname = t.name(id);
        consume_str(rname);
        // Найденный идентификатор, а не единица: единица не зависит от содержимого таблицы, и
        // подмена результата поиска осталась бы стоку незаметна.
        const std::optional<gr::RegionId> found = rname != nullptr ? t.find(rname) : std::nullopt;
        if (found) consume(static_cast<uint64_t>(*found));
        const std::optional<gr::AtlasRegion> r = t.region(id);
        if (r) consume_all(r->x, r->y, r->w, r->h, r->pivot.x.raw);
    }
    return true;
}

// --- профиль движения (LNFM) ---

std::vector<uint8_t> seed_profiles() {
    std::vector<uint8_t> bytes;
    ch::ProfileBakeError err;
    ch::bake_profiles(PROFILE_SRC, bytes, err);
    return bytes;
}

bool read_profiles(const uint8_t* data, size_t size) {
    ch::ProfileTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        const char* pname = t.name(i);
        consume_str(pname);
        ch::MoveProfile prof{};
        if (t.at(i, prof)) consume_all(prof.max_speed.raw, prof.gravity_fall.raw);
        ch::MoveProfile by_name{};
        if (pname != nullptr && t.find(pname, by_name)) consume(by_name.max_speed.raw);
    }
    return true;
}

const Target TARGETS[] = {
    {"input-presets", seed_presets, read_presets},
    {"tilemap", seed_maps, read_maps},
    {"atlas", seed_atlas, read_atlas},
    {"move-profile", seed_profiles, read_profiles},
};

} // namespace

const Target* framework_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
