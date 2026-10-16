#include "rumble_demo.hpp"

#include "rumble_controls.hpp"
#include "rumble_coop_script.hpp"

namespace rumble {

PlayerCommand demo_p1(uint32_t t) {
    PlayerCommand c;
    framework::brawl::BrawlInput& in = c.input;
    in.present = true;
    const bool right = t < 44 || (t >= 110 && t < 140) || (t >= 170 && t < 200) || t == 304 || (t >= 306 && t < 378);
    const bool left = (t >= 244 && t < 300) || (t >= 378 && t < 391);
    in.move.move_x = fix32::from_int(right ? 1 : left ? -1 : 0);
    in.move.move_z = fix32::from_int(t < 12 ? -1 : 0);
    if (t == 44 || t == 56 || t == 68) c.attack = Attack::Punch;
    if (t >= 100 && t < 110) in.buttons = framework::brawl::button::BLOCK;
    if (t == 140 || t == 180) in.buttons = framework::brawl::button::JUMP;
    if (t == 352) in.buttons = framework::brawl::button::DODGE;
    if (t == 141) c.attack = Attack::Kick;
    if (t == 142) c.attack = Attack::Cross;
    if (t == 143 || t == 181) c.attack = Attack::Punch;
    if (t == 200) c.attack = Attack::Cross;
    if (t == 229) c.attack = Attack::Punch;
    if (t == 312) c.attack = Attack::Kick;
    if (t == 408) c.attack = Attack::Grab;
    return c;
}

void demo_p2(input::InputEngine& engine, uint32_t t) {
    if (t == 2 || t == 3) post_key(engine, KEY_KP1, t == 2, t);
    if (t == 31 || t == 41) post_key(engine, KEY_KP3, t == 31, t);
}

} // namespace rumble
