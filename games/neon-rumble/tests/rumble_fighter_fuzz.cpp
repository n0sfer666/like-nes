#include "rumble_fighter_fuzz.hpp"
#include "fighter_bake.hpp"
#include "fighter_read.hpp"
#include "rumble_fighter_fixture.hpp"
#include "fuzz_target.hpp"

namespace fuzz {
namespace br = framework::brawl;

std::vector<uint8_t> seed_fighter() {
    std::vector<uint8_t> bytes;
    br::FighterBakeError err;
    br::bake_fighter("banderas.fighter", br::test::FIGHTER_TEXT, br::test::fixture_clips(), bytes, err);
    return bytes;
}

bool read_fighter(const uint8_t* data, size_t size) {
    br::FighterTable t;
    if (!t.open(data, size)) return false;
    consume_str(t.name());
    consume_str(t.sheet());
    const br::DepthProfile p = t.profile();
    consume_all(p.speed_x.raw, p.speed_z.raw, p.gravity.raw, p.jump_vy.raw, t.run_x().raw, t.depth().raw, t.hp(),
                t.down_ticks(), t.getup_ticks(), t.buffer_ticks(), t.run_tap_ticks(), t.dodge_ticks());
    for (uint32_t i = 0; i < t.move_count(); ++i) {
        consume_str(t.move_name(i));
        consume_str(t.move_clip(i));
        br::Strike s;
        consume(t.move(i, s));
        consume_all(s.box, static_cast<uint8_t>(s.type), s.hits_down, s.slides, s.damage, s.depth.raw, s.hitstop,
                    s.hitstun, s.knock_x.raw, s.knock_y.raw);
    }
    return true;
}

} // namespace fuzz
