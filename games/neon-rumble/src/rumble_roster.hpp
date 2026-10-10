#pragma once
#include <cstdint>

namespace rumble {

constexpr uint32_t FIGHTERS = 3;
constexpr uint32_t PLAYER = 0;
constexpr uint32_t DUMMY = 2;

struct RosterEntry {
    const char* fighter;
    const char* spawn;
    uint8_t team;
};

constexpr RosterEntry ROSTER[FIGHTERS] = {{"banderas", "player", 0}, {"rainbird", "rainbird", 0}, {"adler", "adler", 1}};

constexpr uint32_t sheet_texture(uint32_t level_textures, uint32_t fighter) { return level_textures + fighter; }
constexpr uint32_t solid_texture(uint32_t level_textures) { return level_textures + FIGHTERS; }
constexpr uint32_t atlas_texture(uint32_t level_textures) { return level_textures + FIGHTERS + 1; }

} // namespace rumble
