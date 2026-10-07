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
    const std::string name = std::string(f.name) + "/" + tag;
    if (br::clip_index(f.clips, name.c_str(), out) && br::is_move(a, out)) return true;
    std::fprintf(stderr, "neon-rumble: %s is not a move of %s_fighter, its key would do nothing\n", name.c_str(),
                 f.name);
    return false;
}

} // namespace

bool Kinds::open(const Level& level, const Fighters& fighters) {
    for (uint32_t i = 0; i < FIGHTERS; ++i)
        if (!open_kind(level, fighters[i], types[i], hp[i])) return false;
    const Fighter& f = fighters[PLAYER];
    const br::Archetype& a = types[PLAYER];
    return key_move(f, a, "jab", player.jab) && key_move(f, a, "cross", player.cross) &&
           key_move(f, a, "kick", player.kick) &&
           key_move(f, a, "jump_kick", player.jump_kick);
}

} // namespace rumble
