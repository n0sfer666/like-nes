#include "rumble_kinds.hpp"

#include <cstdio>
#include <string>

#include "fighter_read.hpp"
#include "rumble_level.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;

bool open_kind(const Level& level, const Fighter& f, br::Archetype& out, int32_t& hp) {
    const std::string table = std::string(f.name) + "_fighter";
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!level.read_table(table.c_str(), data, size)) return false;
    br::FighterTable t;
    if (!t.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: %s table: does not open\n", table.c_str());
        return false;
    }
    if (std::string(t.sheet()) != f.name) {
        std::fprintf(stderr, "neon-rumble: %s: sheet %s is not %s\n", table.c_str(), t.sheet(), f.name);
        return false;
    }
    std::string error;
    if (!br::make_archetype(t, f.clips, out, error)) {
        std::fprintf(stderr, "neon-rumble: %s: %s\n", table.c_str(), error.c_str());
        return false;
    }
    hp = static_cast<int32_t>(t.hp());
    return true;
}

bool key_move(const Fighter& f, const br::Archetype& a, const char* tag, uint16_t& out) {
    if (br::find_named(a, tag, out)) return true;
    std::fprintf(stderr, "neon-rumble: %s/%s is not a move of %s_fighter, its key would do nothing\n", f.name, tag,
                 f.name);
    return false;
}

bool player_moves(const Fighter& f, const br::Archetype& a, PlayerMoves& m) {
    return key_move(f, a, "jab", m.jab) && key_move(f, a, "cross", m.cross) && key_move(f, a, "kick", m.kick) &&
           key_move(f, a, "jump_kick", m.jump_kick) && key_move(f, a, "run_kick", m.run_kick) &&
           key_move(f, a, "grab", m.grab);
}

} // namespace

bool Kinds::open(const Level& level, const Fighters& fighters) {
    for (uint32_t i = 0; i < FIGHTERS; ++i)
        if (!open_kind(level, fighters[i], types[i], hp[i])) return false;
    for (uint32_t p = 0; p < moves.size(); ++p)
        if (!player_moves(fighters[p], types[p], moves[p])) return false;
    return true;
}

} // namespace rumble
