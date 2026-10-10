#include "framework_lobby_rig.hpp"

// Спека #25 В4в: выдернутый пад ставит паузу без потери места, возврат — любым свободным падом.
namespace {

using namespace framework::input;
using namespace framework::input::test;

void test_a_pulled_pad_pauses_and_keeps_the_seat() {
    Rig r;
    r.plug(0);
    r.plug(1);
    check(r.lobby.join(0, {0, false}) && r.lobby.join(1, {1, false}), "two players on two pads");
    r.step();
    r.unplug(1);
    r.step();
    check(r.lobby.lost(1) && r.lobby.paused() && r.lobby.present(1), "a pulled pad pauses and keeps P2 present");
    r.button(0, ::input::code::PadA, true);
    r.keypress(KEY_KP1, true);
    r.step();
    check(r.lobby.lost(1) && r.map.assignment(0).pad_slot == 0, "neither P1's pad nor a key brings P2 back");
    r.plug(3);
    r.step();
    check(r.lobby.lost(1), "a pad that only connected brings nobody back");
    r.button(3, ::input::code::PadB, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Resuming && r.lobby.paused(), "the pause holds through the resuming tick");
    r.step();
    check(!r.lobby.paused() && r.lobby.present(1) && r.map.assignment(1).pad_slot == 3,
          "P2 is back on the pad the OS gave another index");
}

void test_the_lost_pad_returns_first_and_on_any_index() {
    Rig r;
    r.plug(1);
    check(r.lobby.join(1, {1, false}), "P2 on pad 1, P1's seat free");
    r.unplug(1);
    r.step();
    r.plug(1);
    r.plug(2);
    r.button(2, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Resuming && r.lobby.state(0) == SeatState::Free,
          "a pad pressed during a pause brings the lost player back before seating a free one");

    Rig same;
    same.plug(1);
    check(same.lobby.join(1, {1, false}), "P2 on pad 1");
    same.unplug(1);
    same.step();
    same.plug(1);
    same.button(1, ::input::code::PadA, true);
    same.step();
    check(same.lobby.state(1) == SeatState::Resuming && same.map.assignment(1).pad_slot == 1,
          "the pad returning on its old index brings its player back");
}

void test_a_pad_and_keyboard_player_keeps_the_keyboard() {
    Rig r;
    r.plug(0);
    check(r.lobby.join(0, {0, true}), "P1 on pad 0 and the keyboard");
    r.unplug(0);
    r.step();
    check(r.lobby.lost(0) && r.map.assignment(0).use_kbd_mouse, "losing the pad keeps P1's keyboard");
    r.plug(2);
    r.button(2, ::input::code::PadA, true);
    r.step();
    const ::input::PlayerAssign a = r.map.assignment(0);
    check(a.pad_slot == 2 && a.use_kbd_mouse, "the returning pad joins P1's keyboard again");
}

void test_a_pad_pulled_right_after_joining_pauses_at_once() {
    Rig r;
    r.plug(0);
    r.button(0, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Joining, "P1 joins on pad 0");
    r.unplug(0);
    r.step();
    check(r.lobby.lost(0) && r.lobby.paused(), "the pad pulled on the first present tick pauses on that tick");
}

} // namespace

int main() {
    std::printf("framework: hotseat lobby pause\n");
    test_a_pulled_pad_pauses_and_keeps_the_seat();
    test_the_lost_pad_returns_first_and_on_any_index();
    test_a_pad_and_keyboard_player_keeps_the_keyboard();
    test_a_pad_pulled_right_after_joining_pauses_at_once();
    return verdict("framework-lobby-pause");
}
