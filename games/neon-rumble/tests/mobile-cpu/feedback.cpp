#include <array>
#include <cstdio>

#include "codes.hpp"
#include "platform_args.hpp"
#include "rumble_feedback.hpp"

namespace {
struct Pulse { haptic::Target target; uint8_t strength; uint16_t ms; };
std::array<Pulse, 8> output;
int count, fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("feedback: FAIL %s\n", why); } }
}
void haptic::pulse(Target target, uint8_t strength, uint16_t ms) { if (count < 8) output[count++] = {target, strength, ms}; }

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    rumble::Hotseat hotseat;
    check(hotseat.open(true), "mobile lobby opens");
    auto& engine = hotseat.engine();
    engine.post({input::RawKind::TouchButtonDown, input::DeviceKind::Touch, 0, input::code::TouchA, 0, 1});
    engine.post({input::RawKind::DeviceConnected, input::DeviceKind::Gamepad, 3, 0, 0, 2});
    engine.post({input::RawKind::PadButtonDown, input::DeviceKind::Gamepad, 3, input::code::PadX, 0, 3});
    rumble::Players commands;
    (void)hotseat.tick(1, commands); (void)hotseat.tick(2, commands);
    const int touch = hotseat.touch_player(), pad = touch == 0 ? 1 : 0;
    check(touch >= 0 && hotseat.assignment(pad).pad_slot == 3, "real lobby assigns distinct local targets");
    if (touch < 0) return 1;
    rumble::Brawl brawl;
    brawl.seats.at[touch] = {true, {12}};
    brawl.seats.at[pad] = {true, {44}};
    const framework::brawl::HitEvent hit{.attacker = {12}, .target = {44}, .kept = true};
    brawl.events.push(hit); brawl.events.push(hit);
    rumble::hit_feedback(brawl, hotseat);
    check(count == 2, "multiple hitboxes coalesce per local player");
    for (int i = 0; i < count; ++i) {
        if (output[i].target.kind == haptic::Kind::Phone)
            check(output[i].strength == 89 && output[i].ms == 20, "touch attacker gets phone feedback");
        else check(output[i].target.pad_slot == 3 && output[i].strength == 179 && output[i].ms == 40, "hurt local pad gets own motor");
    }
    count = 0;
    brawl.events.clear(); brawl.events.push({.attacker = {12}, .target = {44}, .kept = false});
    rumble::hit_feedback(brawl, hotseat);
    check(count == 0, "discarded hit has no feedback");
    brawl.events.clear(); brawl.events.push({.attacker = {98}, .target = {99}, .kept = true});
    rumble::hit_feedback(brawl, hotseat);
    check(count == 0, "remote/NPC-only hit has no local motor");
    engine.post({input::RawKind::DeviceDisconnected, input::DeviceKind::Gamepad, 3, 0, 0, 4});
    (void)hotseat.tick(3, commands);
    brawl.events.clear(); brawl.events.push({.attacker = {98}, .target = {44}, .kept = true});
    rumble::hit_feedback(brawl, hotseat);
    check(count == 0, "lost local device excluded");
    brawl.events.clear(); brawl.events.push({.attacker = {98}, .target = {12}, .kept = true});
    brawl.seats.at[touch].present = false;
    rumble::hit_feedback(brawl, hotseat);
    check(count == 0, "absent simulation seat excluded");
    std::puts(fails ? "feedback: FAIL" : "feedback: PASS local targets, kept hits, coalescing, lost/absent exclusions");
    return fails ? 1 : 0;
}
