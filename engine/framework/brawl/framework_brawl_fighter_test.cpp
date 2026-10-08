#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include "fighter_bake.hpp"
#include "fighter_format.hpp"
#include "fighter_read.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_fighter_fixture.hpp"
#include "hash_mix.hpp"
#include "section_format.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::same_strike;

constexpr uint64_t GOLDEN = 0x1a8d5bbe7e6bbfb6ull;

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = framework::physics::FNV_OFFSET;
    framework::physics::mix_bytes(h, b.data(), b.size());
    return h;
}

template <class T>
T load(const std::vector<uint8_t>& b, std::size_t at) {
    T v{};
    std::memcpy(&v, b.data() + at, sizeof(T));
    return v;
}

template <class T>
void store(std::vector<uint8_t>& b, std::size_t at, T v) {
    std::memcpy(b.data() + at, &v, sizeof(T));
}

void round_trip(const std::vector<uint8_t>& baked) {
    FighterTable t;
    check(t.open(baked.data(), baked.size()), "baked fighter opens");
    check(std::strcmp(t.name(), "banderas.fighter") == 0, "name");
    check(std::strcmp(t.sheet(), "banderas") == 0, "sheet");
    const DepthProfile p = t.profile();
    check(p.speed_x == fix32::from_int(2) && p.speed_z == fix32::from_int(1), "speed_x, speed_z");
    check(p.gravity == fix32::from_raw(1 << 15) && p.jump_vy == fix32::from_int(6), "gravity, jump_vy");
    check(t.run_x() == fix32::from_int(3) && t.depth() == fix32::from_int(4), "run_x, depth");
    check(t.hp() == 120, "hp");
    check(t.down_ticks() == 30 && t.getup_ticks() == 24, "down, getup");
    check(t.move_count() == 2, "two moves");
    const Strike jab{0, HitType::Light, false, 8, fix32::from_int(5), 4, 12, fix32::from_raw(3 << 15), fix32{}};
    const Strike kick{1,  HitType::Launch,          true, 20, fix32::from_raw((25 << 16) / 4), 7, 30,
                      fix32::from_int(3), fix32::from_raw((19 << 16) / 4)};
    Strike s;
    check(std::strcmp(t.move_clip(0), "banderas/jab") == 0 && t.move(0, s) && same_strike(s, jab), "move jab");
    check(std::strcmp(t.move_clip(1), "banderas/kick") == 0 && t.move(1, s) && same_strike(s, kick), "move kick");
    check(!t.move(2, s) && t.move_clip(2)[0] == '\0', "move past the end is refused");
}

enum class At { Header, Row, Strike };

struct Corruption {
    const char* what;
    At at;
    std::size_t offset;
    std::size_t width;
    uint32_t value;
};

const uint32_t OVER_SPEED = static_cast<uint32_t>(MAX_FIGHTER_SPEED.raw + 1);
const uint32_t UNDER_KNOCK = static_cast<uint32_t>(-MAX_FIGHTER_SPEED.raw - 1);
const uint32_t OVER_DEPTH = static_cast<uint32_t>(MAX_FIGHTER_DEPTH.raw + 1);

const Corruption CORRUPTIONS[] = {
    {"version 1", At::Header, offsetof(framework::core::SectionHeader, version), 4, 1},
    {"count 2", At::Header, offsetof(framework::core::SectionHeader, count), 4, 2},
    {"hp over the cap", At::Row, offsetof(FighterRow, hp), 4, MAX_HP + 1},
    {"zero hp", At::Row, offsetof(FighterRow, hp), 4, MIN_HP - 1},
    {"zero down", At::Row, offsetof(FighterRow, down_ticks), 4, 0},
    {"down over the cap", At::Row, offsetof(FighterRow, down_ticks), 4, MAX_HIT_TICKS + 1},
    {"zero getup", At::Row, offsetof(FighterRow, getup_ticks), 4, 0},
    {"getup over the cap", At::Row, offsetof(FighterRow, getup_ticks), 4, MAX_HIT_TICKS + 1},
    {"zero gravity", At::Row, offsetof(FighterRow, gravity_raw), 4, 0},
    {"gravity over the cap", At::Row, offsetof(FighterRow, gravity_raw), 4, OVER_SPEED},
    {"negative speed", At::Row, offsetof(FighterRow, speed_x_raw), 4, 0xffffffffu},
    {"speed over the cap", At::Row, offsetof(FighterRow, speed_x_raw), 4, OVER_SPEED},
    {"negative speed_z", At::Row, offsetof(FighterRow, speed_z_raw), 4, 0xffffffffu},
    {"speed_z over the cap", At::Row, offsetof(FighterRow, speed_z_raw), 4, OVER_SPEED},
    {"negative run_x", At::Row, offsetof(FighterRow, run_x_raw), 4, 0xffffffffu},
    {"run_x over the cap", At::Row, offsetof(FighterRow, run_x_raw), 4, OVER_SPEED},
    {"negative jump_vy", At::Row, offsetof(FighterRow, jump_vy_raw), 4, 0xffffffffu},
    {"jump_vy over the cap", At::Row, offsetof(FighterRow, jump_vy_raw), 4, OVER_SPEED},
    {"zero depth", At::Row, offsetof(FighterRow, depth_raw), 4, 0},
    {"depth over the cap", At::Row, offsetof(FighterRow, depth_raw), 4, OVER_DEPTH},
    {"name past the strings", At::Row, offsetof(FighterRow, name_offset), 4, 0xffffu},
    {"moves past the end", At::Row, offsetof(FighterRow, move_count), 4, 9999},
    {"box 4", At::Strike, offsetof(StrikeRow, box), 1, MAX_HIT_BOXES},
    {"type 5", At::Strike, offsetof(StrikeRow, type), 1, HIT_TYPE_LAST + 1u},
    {"unknown flag", At::Strike, offsetof(StrikeRow, flags), 1, 2},
    {"pad set", At::Strike, offsetof(StrikeRow, pad), 1, 1},
    {"damage over the cap", At::Strike, offsetof(StrikeRow, damage), 4, MAX_DAMAGE + 1},
    {"zero strike depth", At::Strike, offsetof(StrikeRow, depth_raw), 4, 0},
    {"strike depth over the cap", At::Strike, offsetof(StrikeRow, depth_raw), 4, OVER_DEPTH},
    {"hitstop over the cap", At::Strike, offsetof(StrikeRow, hitstop), 4, MAX_HIT_TICKS + 1},
    {"hitstun over the cap", At::Strike, offsetof(StrikeRow, hitstun), 4, MAX_HIT_TICKS + 1},
    {"knock_x over the cap", At::Strike, offsetof(StrikeRow, knock_x_raw), 4, OVER_SPEED},
    {"knock_y over the cap", At::Strike, offsetof(StrikeRow, knock_y_raw), 4, OVER_SPEED},
    {"knock_x under the floor", At::Strike, offsetof(StrikeRow, knock_x_raw), 4, UNDER_KNOCK},
    {"knock_y under the floor", At::Strike, offsetof(StrikeRow, knock_y_raw), 4, UNDER_KNOCK},
};

bool refused(const std::vector<uint8_t>& bad) {
    FighterTable t;
    const bool opened = t.open(bad.data(), bad.size());
    return !opened && !t.valid() && t.move_count() == 0 && t.name()[0] == '\0';
}

void reader_refusals(const std::vector<uint8_t>& baked) {
    const std::size_t row = load<uint32_t>(baked, offsetof(framework::core::SectionHeader, rows_offset));
    const std::size_t strike = load<uint32_t>(baked, row + offsetof(FighterRow, move_offset)) + sizeof(StrikeRow);
    for (const Corruption& c : CORRUPTIONS) {
        std::vector<uint8_t> bad = baked;
        const std::size_t at = c.offset + (c.at == At::Row ? row : c.at == At::Strike ? strike : 0);
        if (c.width == 1) bad[at] = static_cast<uint8_t>(c.value);
        else store<uint32_t>(bad, at, c.value);
        check(refused(bad), c.what);
    }
    std::vector<uint8_t> bad = baked;
    const std::size_t strings = load<uint32_t>(baked, offsetof(framework::core::SectionHeader, strings_offset));
    bad[strings + load<uint32_t>(baked, row + offsetof(FighterRow, sheet_offset))] = '\0';
    check(refused(bad), "empty sheet");
    bad = baked;
    bad[strings + load<uint32_t>(baked, strike + offsetof(StrikeRow, clip_offset))] = '\0';
    check(refused(bad), "empty move clip");
    bad = baked;
    bad.pop_back();
    check(refused(bad), "truncated section");
}

} // namespace

int main() {
    std::vector<uint8_t> baked;
    FighterBakeError err;
    const std::vector<framework::graphics::ClipSrc> clips = test::fixture_clips();
    if (!bake_fighter("banderas.fighter", test::FIGHTER_TEXT, clips, baked, err)) {
        std::printf("  FAIL: bake: line %d: %s\n", err.line, err.message.c_str());
        return test::verdict("framework-brawl-fighter");
    }
    const uint64_t h = hash_bytes(baked);
    std::printf("fighter table: %zu bytes, hash 0x%016" PRIx64 "\n", baked.size(), h);
    check(h == GOLDEN, "byte golden of the fighter table");
    round_trip(baked);
    reader_refusals(baked);
    return test::verdict("framework-brawl-fighter");
}
