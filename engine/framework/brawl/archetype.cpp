#include "archetype.hpp"

#include <cstring>
#include <iterator>

namespace framework::brawl {

namespace {

bool sheet_clip(const graphics::ClipTable& clips, const std::string& sheet, const char* tag, uint16_t& out,
                std::string& error) {
    const std::string name = sheet + "/" + tag;
    if (clip_index(clips, name.c_str(), out)) return true;
    error = "no clip '" + name + "' in the clips";
    return false;
}

} // namespace

bool clip_index(const graphics::ClipTable& clips, const char* name, uint16_t& out) {
    const uint32_t n = clips.count() < 0xffffu ? clips.count() : 0xffffu;
    for (uint32_t i = 0; i < n; ++i) {
        if (std::strcmp(clips.name(i), name) != 0) continue;
        out = static_cast<uint16_t>(i);
        return true;
    }
    return false;
}

bool make_archetype(const FighterTable& fighter, const graphics::ClipTable& clips, Archetype& out,
                    std::string& error) {
    out = Archetype{};
    if (!fighter.valid() || !clips.valid()) {
        error = "the fighter table or the clips do not open";
        return false;
    }
    const std::string sheet = fighter.sheet();
    const char* const tags[] = {"idle", "walk", "jump", "hurt", "fall", "down", "getup"};
    uint16_t* const slots[] = {&out.idle, &out.walk, &out.jump, &out.hurt, &out.fall, &out.down, &out.getup};
    for (uint32_t i = 0; i < std::size(tags); ++i)
        if (!sheet_clip(clips, sheet, tags[i], *slots[i], error)) return false;
    if (fighter.move_count() > MAX_MOVES) {
        error = "more than " + std::to_string(MAX_MOVES) + " moves";
        return false;
    }
    for (uint32_t i = 0; i < fighter.move_count(); ++i) {
        MoveSlot& m = out.moves[i];
        if (!clip_index(clips, fighter.move_clip(i), m.clip)) {
            error = std::string("no clip '") + fighter.move_clip(i) + "' in the clips";
            return false;
        }
        if (!fighter.move(i, m.strike)) {
            error = "move " + std::to_string(i) + " does not read";
            return false;
        }
    }
    out.clips = &clips;
    out.profile = fighter.profile();
    out.depth = fighter.depth();
    out.move_count = fighter.move_count();
    out.down_ticks = ticks16(fighter.down_ticks());
    out.getup_ticks = ticks16(fighter.getup_ticks());
    return true;
}

bool is_move(const Archetype& a, uint16_t clip) {
    for (uint32_t i = 0; i < a.move_count; ++i)
        if (a.moves[i].clip == clip) return true;
    return false;
}

bool find_move(const Archetype& a, uint16_t clip, uint8_t box, uint8_t& slot) {
    for (uint32_t i = 0; i < a.move_count; ++i) {
        if (a.moves[i].clip != clip || a.moves[i].strike.box != box) continue;
        slot = static_cast<uint8_t>(i);
        return true;
    }
    return false;
}

} // namespace framework::brawl
