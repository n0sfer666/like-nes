#pragma once
#include <string>

#include "framework_brawl_hit_fixture.hpp"

namespace framework::brawl::test {

struct Grapple {
    Ring r{dummy_clips(), std::string(DUMMY_TEXT) + GRAB_ROWS};
    uint16_t reach = 0;

    Grapple() { clip_index(r.arena.clips, "dummy/reach", reach); }
    Body& reaching(int32_t x, int8_t facing, uint8_t team) {
        Body& b = r.put(x, facing, team);
        r.play(b, reach);
        b.elapsed = 1;
        return b;
    }
    HitEvents hit(bool friendly_fire = false) {
        HitEvents e = r.collect(friendly_fire);
        apply_hits(r.pool, r.arena.kinds, e);
        return e;
    }
    const Archetype& kind() const { return r.arena.kinds[0]; }
};

} // namespace framework::brawl::test
