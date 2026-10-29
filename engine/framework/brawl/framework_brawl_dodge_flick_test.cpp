#include <array>
#include <cstdio>
#include <vector>

#include "codes.hpp"
#include "flick.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_guard_fixture.hpp"
#include "input_engine.hpp"

namespace {

using namespace framework::brawl;
using framework::input::flick;
using framework::input::FlickRule;
using test::check;
using ::input::InputFrame;
using ::input::RawEvent;
using ::input::RawKind;

enum Action : int { BLOCK = 3 };
enum Axis : int { MOVE_X = 1 };
constexpr uint32_t K = 3;
constexpr uint32_t END = 90;
const FlickRule RULE{BLOCK, MOVE_X, fix32::from_int(3) / fix32::from_int(4), K};

fix32 tenths(int n) { return fix32::from_int(n) / fix32::from_int(10); }

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
            dodged.push_back(flick(engine.buffer(0).latest(window), RULE));
        }
    }
};

std::vector<bool> replay(const std::vector<InputFrame>& record) {
    ::input::InputBuffer<::input::BUFFER_TICKS> buffer;
    std::vector<bool> out;
    for (const InputFrame& f : record) {
        buffer.push(f);
        std::array<InputFrame, K + 1> window;
        out.push_back(flick(buffer.latest(window), RULE));
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
    Live live;
    live.play();
    test_the_record_replays_the_dodges(live);
    test_a_backward_flick_rolls_along_facing(live);
    return test::verdict("framework-brawl-dodge-flick");
}
