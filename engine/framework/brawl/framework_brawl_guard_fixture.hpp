#pragma once
#include <string>
#include <vector>

#include "framework_brawl_hit_fixture.hpp"

namespace framework::brawl::test {

inline Command press(uint16_t buttons, int32_t x = 0) {
    Command c;
    c.input.present = true;
    c.input.buttons = buttons;
    c.input.move.move_x = fix32::from_int(x);
    return c;
}

struct Solo {
    Ring r;
    Body* b = nullptr;

    explicit Solo(int8_t facing = 1) { b = &r.put(0, facing, 0); }
    void step(const Command& c) { tick(r.pool, r.arena, {c}); }
    const Archetype& kind() const { return r.arena.kinds[0]; }
};

inline std::string arena_without(const std::string& tag) {
    std::vector<graphics::ClipSrc> src = dummy_clips();
    std::erase_if(src, [&](const graphics::ClipSrc& c) { return c.name == "dummy/" + tag; });
    return Arena(src).error;
}

} // namespace framework::brawl::test
