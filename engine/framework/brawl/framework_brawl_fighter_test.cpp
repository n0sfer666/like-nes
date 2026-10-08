#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "fighter_bake.hpp"
#include "fighter_format.hpp"
#include "fighter_read.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_fighter_corruptions.hpp"
#include "framework_brawl_fighter_fixture.hpp"
#include "hash_mix.hpp"
#include "section_format.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::At;
using test::CORRUPTIONS;
using test::Corruption;
using test::same_strike;

constexpr uint64_t GOLDEN = 0xe74f5abb2384663aull;

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
    check(t.move_count() == 3, "three moves");
    check(t.buffer_ticks() == 7 && t.run_tap_ticks() == 9, "buffer, run_tap");
    check(t.chain_count() == 3 && t.chain_move(0) == 0 && t.chain_move(1) == 0 && t.chain_move(2) == 1,
          "chain jab jab kick by move row");
    const Strike jab{0, HitType::Light, false, false, 8, fix32::from_int(5), 4, 12, fix32::from_raw(3 << 15), fix32{}};
    const Strike kick{1,  HitType::Launch, true, false, 20, fix32::from_raw((25 << 16) / 4), 7, 30,
                      fix32::from_int(3), fix32::from_raw((19 << 16) / 4)};
    const Strike run_kick{1, HitType::Heavy, false, true, 12, fix32::from_int(6), 5, 18, fix32::from_int(2), fix32{}};
    Strike s;
    check(std::strcmp(t.move_name(0), "jab") == 0 && std::strcmp(t.move_clip(0), "banderas/jab") == 0 &&
              t.move(0, s) && same_strike(s, jab),
          "move jab");
    check(std::strcmp(t.move_name(1), "kick") == 0 && std::strcmp(t.move_clip(1), "banderas/kick") == 0 &&
              t.move(1, s) && same_strike(s, kick),
          "move kick");
    check(std::strcmp(t.move_name(2), "run_kick") == 0 && std::strcmp(t.move_clip(2), "banderas/kick") == 0 &&
              t.move(2, s) && same_strike(s, run_kick),
          "a run_kick row plays the kick clip and slides");
    check(!t.move(3, s) && t.move_clip(3)[0] == '\0' && t.move_name(3)[0] == '\0', "move past the end is refused");
}

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
    bad[strings + load<uint32_t>(baked, strike + offsetof(StrikeRow, name_offset))] = '\0';
    check(refused(bad), "empty move name");
    bad = baked;
    store<uint32_t>(bad, load<uint32_t>(baked, row + offsetof(FighterRow, chain_offset)), 3);
    check(refused(bad), "chain step past the moves");
    bad = baked;
    bad.pop_back();
    check(refused(bad), "truncated section");
}

bool opens_with_chain_at(const std::vector<uint8_t>& baked, uint32_t count) {
    std::vector<uint8_t> bad = baked;
    const std::size_t row = load<uint32_t>(baked, offsetof(framework::core::SectionHeader, rows_offset));
    const uint32_t at = load<uint32_t>(baked, row + offsetof(FighterRow, chain_offset)) - sizeof(uint32_t);
    store<int32_t>(bad, at, 0);
    store<uint32_t>(bad, row + offsetof(FighterRow, chain_offset), at);
    store<uint32_t>(bad, row + offsetof(FighterRow, chain_count), count);
    FighterTable t;
    return t.open(bad.data(), bad.size()) && t.chain_count() == count;
}

void chain_cap(const std::vector<framework::graphics::ClipSrc>& clips) {
    std::string text = test::FIGHTER_TEXT;
    const std::string from = "jab  jab kick";
    text.replace(text.find(from), from.size(), "jab jab jab jab jab jab jab kick");
    std::vector<uint8_t> baked;
    FighterBakeError err;
    const bool ok = bake_fighter("banderas.fighter", text, clips, baked, err);
    check(ok && opens_with_chain_at(baked, MAX_CHAIN), "control: a chain moved onto a zeroed knock_y opens");
    check(ok && !opens_with_chain_at(baked, MAX_CHAIN + 1), "a chain one step over the cap");
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
    chain_cap(clips);
    return test::verdict("framework-brawl-fighter");
}
