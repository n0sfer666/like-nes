#include <array>
#include <cstdio>
#include <vector>

#include "codes.hpp"
#include "dodge_flick.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_guard_fixture.hpp"
#include "input_engine.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using ::input::InputFrame;
using ::input::RawEvent;
using ::input::RawKind;

enum Action : int { BLOCK = 3 };
enum Axis : int { MOVE_X = 1 };
constexpr uint32_t K = 3;
constexpr uint32_t END = 90;
const fix32 ONE = fix32::from_int(1);
const FlickRule RULE{BLOCK, MOVE_X, fix32::from_int(3) / fix32::from_int(4), K};

fix32 tenths(int n) { return fix32::from_int(n) / fix32::from_int(10); }

InputFrame frame(fix32 x, bool block = true) {
    InputFrame f;
    f.axes[MOVE_X] = x;
    if (block) f.held = uint64_t{1} << BLOCK;
    return f;
}

bool flicks(const std::vector<InputFrame>& frames) { return dodge_flick(frames, RULE); }

void test_a_flick_within_k_ticks_dodges() {
    const fix32 t = RULE.threshold;
    check(flicks({frame({}), frame(tenths(3)), frame(tenths(6)), frame(t)}), "rest to the threshold in exactly K ticks dodges");
    check(!flicks({frame({}), frame(tenths(2)), frame(tenths(4)), frame(tenths(6)), frame(t)}),
          "one tick slower than K does not dodge, though the rest frame is in the span");
    check(flicks({frame({}), frame(ONE)}), "a keyboard axis, rest to full in one tick, dodges");
    check(flicks({frame({}), frame(-t)}), "a flick to the left dodges");
}

void test_the_threshold_is_inclusive() {
    const fix32 below = fix32::from_raw(RULE.threshold.raw - 1);
    check(!flicks({frame({}), frame(below)}), "one raw step under the threshold does not dodge");
    check(!flicks({frame({}), frame(-below)}), "one raw step under the threshold to the left does not dodge");
}

void test_block_is_held_on_the_flick_tick() {
    check(!flicks({frame({}), frame(ONE, false)}), "a flick without block does not dodge");
    check(!flicks({frame({}, false), frame(ONE, false), frame(ONE)}), "block pressed after the axis crossed does not dodge");
    check(flicks({frame({}, false), frame(ONE)}), "block pressed on the flick tick dodges");
}

void test_a_flick_dodges_once() {
    check(!flicks({frame({}), frame(ONE), frame(ONE)}), "holding the axis past the threshold does not dodge again");
    check(!flicks({frame(-ONE), frame(ONE)}), "a swing across without rest is not a flick");
    check(!flicks({frame({}), frame(-ONE), frame(tenths(-5)), frame(tenths(8))}),
          "a full swing left and back right within K ticks is not a flick from rest");
    check(!flicks({frame(ONE)}) && !flicks({}), "a span without a previous tick does not dodge");
}

void test_a_rule_out_of_range_never_dodges() {
    const std::vector<InputFrame> frames{frame({}), frame(ONE)};
    const fix32 t = RULE.threshold;
    check(!dodge_flick(frames, FlickRule{BLOCK, ::input::MAX_AXES, t, K}) &&
              !dodge_flick(frames, FlickRule{BLOCK, -1, t, K}),
          "an axis outside the frame never dodges");
    check(!dodge_flick(frames, FlickRule{64, MOVE_X, t, K}) && !dodge_flick(frames, FlickRule{-1, MOVE_X, t, K}),
          "a block action outside the bitset never dodges");
}

struct Live {
    ::input::ActionMap map;
    ::input::InputEngine engine{map};
    std::vector<bool> dodged;
    uint64_t seq = 0;

    Live() {
        ::input::ActionLayout l;
        l.bind(BLOCK, {::input::SourceKind::PadButton, ::input::code::LB, 1});
        l.bind_axis(MOVE_X, {::input::SourceKind::PadAxis, ::input::code::LX, 1}, {}, tenths(3));
        check(map.set_layout(0, l) && map.assign_player(0, {.pad_slot = 0}), "the pad seat takes its layout");
        engine.set_recording(true);
    }

    void pad(RawKind k, uint16_t code = 0, int32_t value = 0) {
        engine.post(RawEvent{k, ::input::DeviceKind::Gamepad, 0, code, value, seq++});
    }
    void stick(int n) { pad(RawKind::PadAxis, ::input::code::LX, tenths(n).raw); }

    void script(uint32_t t) {
        const std::array<std::pair<uint32_t, int>, 14> sticks{{{20, 2}, {21, 5}, {22, 9}, {30, 0}, {40, 2}, {41, 4},
                                                               {42, 5}, {43, 7}, {44, 9}, {50, 0}, {60, -10},
                                                               {65, 0}, {70, -1}, {71, -10}}};
        if (t == 1) pad(RawKind::DeviceConnected);
        if (t == 10 || t == 68) pad(RawKind::PadButtonDown, ::input::code::LB);
        if (t == 55) pad(RawKind::PadButtonUp, ::input::code::LB);
        for (const auto& [at, n] : sticks)
            if (at == t) stick(n);
    }

    void play() {
        for (uint32_t t = 0; t < END; ++t) {
            script(t);
            engine.drain();
            engine.resolve(t, 0);
            std::array<InputFrame, K + 1> window;
            dodged.push_back(dodge_flick(engine.buffer(0).latest(window), RULE));
        }
    }
};

std::vector<bool> replay(const std::vector<InputFrame>& record) {
    ::input::InputBuffer<::input::BUFFER_TICKS> buffer;
    std::vector<bool> out;
    for (const InputFrame& f : record) {
        buffer.push(f);
        std::array<InputFrame, K + 1> window;
        out.push_back(dodge_flick(buffer.latest(window), RULE));
    }
    return out;
}

void test_the_record_replays_the_dodges(const Live& live) {
    std::vector<uint32_t> at;
    for (uint32_t t = 0; t < live.dodged.size(); ++t)
        if (live.dodged[t]) at.push_back(t);
    check(at == std::vector<uint32_t>{22, 71},
          "a pad flicks on ticks 22 and 71; the slow tilt and the flick without block do not dodge");
    check(live.engine.record(0).size() == END && replay(live.engine.record(0)) == live.dodged,
          "the record replays every dodge on its tick");
}

void test_a_backward_flick_rolls_along_facing(const Live& live) {
    test::Solo u(1);
    for (uint32_t t = 66; t <= 71; ++t) {
        const InputFrame& f = live.engine.record(0)[t];
        const uint16_t buttons = static_cast<uint16_t>((f.action_held(BLOCK) ? button::BLOCK : 0) |
                                                       (live.dodged[t] ? button::DODGE : 0));
        u.step(test::press(buttons, f.axes[MOVE_X] < fix32{} ? -1 : 0));
    }
    check(u.b->react == Reaction::Dodge && u.b->facing == 1 && u.b->pos.vx == u.kind().run_x,
          "a flick to the left from the block rolls the body the way it faces");
}

} // namespace

int main() {
    std::printf("brawl: dodge flick\n");
    test_a_flick_within_k_ticks_dodges();
    test_the_threshold_is_inclusive();
    test_block_is_held_on_the_flick_tick();
    test_a_flick_dodges_once();
    test_a_rule_out_of_range_never_dodges();
    Live live;
    live.play();
    test_the_record_replays_the_dodges(live);
    test_a_backward_flick_rolls_along_facing(live);
    return test::verdict("framework-brawl-dodge-flick");
}
