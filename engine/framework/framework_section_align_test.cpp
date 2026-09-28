#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "atlas_bake.hpp"
#include "atlas_format.hpp"
#include "atlas_read.hpp"
#include "map_bake.hpp"
#include "map_format.hpp"
#include "map_read.hpp"
#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "preset_format.hpp"
#include "presets.hpp"
#include "profile_bake.hpp"
#include "profile_format.hpp"
#include "profile_read.hpp"

// Выравнивание БАЗЫ у четырёх читателей zero-parse секций (аудит #21, A·2·6). Каждый принимает
// `const void*` и приводит его к заголовку со своим `alignof`, а выровненность держалась на
// незаписанном контракте «payload бандла выровнен на PAYLOAD_ALIGN»: первый же вызывающий с буфером
// со сдвигом получал бы невыровненное чтение — UB, на strict-align SIGBUS, — а голдены сходились бы.
// Одна цель над модулями, а не строка в каждом refusal-тесте: дефект — одна и та же строка в четырёх
// файлах, и утверждение о нём одно.
namespace {

namespace in = framework::input;
namespace tl = framework::tilemap;
namespace gr = framework::graphics;
namespace ch = framework::character;

int fails = 0;

// Наименьшие законные манифесты: предмет здесь — адрес секции, а не её содержание.
const char* PRESET = "preset | p\naxis | move_x | key:d | key:a\n";
const char* MAP =
    "map | field\ntile_size | 16\norigin | 0 | -32\nlegend | . | empty\nlegend | X | solid\n"
    "row | .X.\nrow | XXX\n";
const char* ATLAS = "atlas | 64 | 32\nregion | first | 0 | 0 | 16 | 8 | 8 | 4\n";
const char* PROFILE =
    "profile | player\nmax_speed | 340\nground_accel | 2400\nground_decel | 3200\n"
    "air_accel | 1600\nair_decel | 900\ngravity_rise | 1200\ngravity_fall | 2400\n"
    "max_fall_speed | 900\njump_height | 64\nmin_jump_height | 16\ncoyote_ticks | 6\n"
    "buffer_ticks | 6\ncorner_correction | 4\nground_snap | 8\nmax_slope | 1\nclimb_speed | 120\n"
    "ladder_regrab_ticks | 8\n";

// Секция кладётся по каждому сдвигу от 0 до `alignof` заголовка включительно, и открыться обязаны
// ровно кратные. Оба конца — позитивный контроль: без нуля отказ на сдвиге 1 читался бы и как
// «читатель не открывает ничего», а сдвиг `alignof` отличает требование заголовка от более строгого.
template <typename Header, typename Table>
void check_shifts(const char* name, bool baked, const std::vector<uint8_t>& bytes) {
    static_assert(alignof(Header) > 1, "a byte-aligned header has no misaligned base to refuse");
    if (!baked) {
        std::printf("  FAIL: the %s fixture did not bake\n", name);
        ++fails;
        return;
    }
    // Точка отсчёта выравнивается здесь же, а не берётся у аллокатора на веру: иначе «сдвиг 0»
    // был бы выровнен так, как повезло куче.
    constexpr std::size_t ROOM = alignof(std::max_align_t);
    std::vector<uint8_t> room(bytes.size() + 2 * ROOM);
    const auto addr = reinterpret_cast<std::uintptr_t>(room.data());
    uint8_t* origin = room.data() + (ROOM - addr % ROOM) % ROOM;
    for (std::size_t shift = 0; shift <= alignof(Header); ++shift) {
        std::memcpy(origin + shift, bytes.data(), bytes.size());
        Table t;
        const bool expected = shift % alignof(Header) == 0;
        if (t.open(origin + shift, bytes.size()) != expected) {
            std::printf("  FAIL: the %s section at shift %u %s\n", name,
                        static_cast<unsigned>(shift), expected ? "was refused" : "opened");
            ++fails;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("section readers refuse a misaligned base\n");

    std::vector<uint8_t> preset, map, atlas, profile;
    in::PresetBakeError preset_err;
    const bool preset_ok = in::bake_presets(PRESET, preset, preset_err);
    check_shifts<in::PresetHeader, in::PresetTable>("preset", preset_ok, preset);

    tl::MapBakeError map_err;
    const bool map_ok = tl::bake_maps(MAP, map, map_err);
    check_shifts<tl::MapHeader, tl::TileMapTable>("map", map_ok, map);

    gr::AtlasBakeError atlas_err;
    const bool atlas_ok = gr::bake_atlas(ATLAS, atlas, atlas_err);
    check_shifts<gr::AtlasHeader, gr::AtlasTable>("atlas", atlas_ok, atlas);

    ch::ProfileBakeError profile_err;
    const bool profile_ok = ch::bake_profiles(PROFILE, profile, profile_err);
    check_shifts<ch::MoveHeader, ch::ProfileTable>("profile", profile_ok, profile);

    std::printf("framework-section-align: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
