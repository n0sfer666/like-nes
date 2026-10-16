#include "framework_lobby_rig.hpp"
#include "player_layout.hpp"

// Спека #26 В2а: экранный тач в лобби. Экран один — его держит одно место; пад, отключённый у игрока
// на таче, не отнимает тач; потерявшему пад место отдаёт первое касание его кнопки, если тач ничей.
namespace {

using namespace framework::input;
using namespace framework::input::test;

void add_touch(Rig& r, int player, uint16_t button) {
    ::input::ActionLayout l = r.map.layout(player);
    l.bind(JUMP, touch_button(button));
    check(r.map.set_layout(player, l), "a touch button joins the layout");
}

void test_a_touch_button_of_the_layout_seats_its_player() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    ::input::ActionLayout l = r.map.layout(0);
    l.bind_axis(1, {::input::SourceKind::TouchAxis, ::input::code::StickX, 1}, {}, fix32{});
    check(r.map.set_layout(0, l), "the stick joins the layout");
    r.stick(::input::code::StickX, fix32::from_int(1));
    r.touch(::input::code::TouchB, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Free, "the stick and a button outside the layout seat nobody");
    r.touch(::input::code::TouchA, true);
    r.step();
    const ::input::PlayerAssign a = r.map.assignment(0);
    check(r.lobby.state(0) == SeatState::Joining && a.use_touch && !a.use_kbd_mouse && a.pad_slot == -1,
          "the layout's touch button seats P1 on the touch alone");
}

void test_one_screen_holds_one_seat() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    add_touch(r, 1, ::input::code::TouchB);
    check(r.lobby.join(0, {.use_touch = true}), "P1 sits at the touch");
    r.touch(::input::code::TouchB, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Free, "P2's touch button does not take the touch from P1");
    ::input::SharedInput shared;
    check(!r.lobby.join(1, {.use_touch = true}, &shared) && !r.map.assignment(1).use_touch,
          "a second seat on the touch is refused");
    check(shared_input_text(shared) == "player 2 and player 1 share the touch screen",
          "the refusal names the screen, not a button");
}

void test_a_pulled_pad_keeps_the_touch() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    r.plug(0);
    check(r.lobby.join(0, {.pad_slot = 0, .use_touch = true}), "P1 on pad 0 and the touch");
    r.step();
    r.unplug(0);
    r.step();
    check(r.lobby.lost(0) && r.map.assignment(0).use_touch, "the lost seat keeps its touch");
    r.touch(::input::code::TouchA, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Resuming && r.map.assignment(0).pad_slot == -1,
          "its own touch brings P1 back without a pad");
}

void test_the_lost_seat_takes_a_free_touch_first() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    add_touch(r, 1, ::input::code::TouchA);
    r.plug(1);
    check(r.lobby.join(1, {.pad_slot = 1}), "P2 on pad 1, P1's seat free");
    r.unplug(1);
    r.step();
    r.touch(::input::code::TouchA, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Resuming && r.map.assignment(1).use_touch &&
              r.lobby.state(0) == SeatState::Free,
          "the touch brings the lost player back before seating a free one");
}

void test_a_touch_held_by_another_seat_brings_nobody_back() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    add_touch(r, 1, ::input::code::TouchA);
    r.plug(1);
    check(r.lobby.join(0, {.use_touch = true}) && r.lobby.join(1, {.pad_slot = 1}), "P1 on the touch, P2 on pad 1");
    r.unplug(1);
    r.step();
    r.touch(::input::code::TouchA, true);
    r.step();
    check(r.lobby.lost(1) && r.map.assignment(0).use_touch && !r.map.assignment(1).use_touch,
          "P1's touch does not bring P2 back");
}

void test_a_returning_pad_keeps_the_touch() {
    Rig r;
    r.plug(0);
    check(r.lobby.join(0, {.pad_slot = 0, .use_touch = true}), "P1 on pad 0 and the touch");
    r.step();
    r.unplug(0);
    r.step();
    r.plug(2);
    r.button(2, ::input::code::PadA, true);
    r.step();
    const ::input::PlayerAssign a = r.map.assignment(0);
    check(r.lobby.state(0) == SeatState::Resuming && a.pad_slot == 2 && a.use_touch,
          "the pad brings P1 back with the touch still theirs");
}

// Порядок захвата в одном тике — пад, клавиши, тач — держит только последовательность вызовов в
// Lobby::update; эти три случая падают от её перестановки.
void test_a_pad_and_a_touch_in_one_tick_seat_two() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    add_touch(r, 1, ::input::code::TouchA);
    r.plug(1);
    check(r.lobby.join(1, {.pad_slot = 1}), "P2 on pad 1");
    r.unplug(1);
    r.step();
    r.plug(2);
    r.button(2, ::input::code::PadA, true);
    r.touch(::input::code::TouchA, true);
    r.step();
    const ::input::PlayerAssign p2 = r.map.assignment(1);
    check(r.lobby.state(1) == SeatState::Resuming && p2.pad_slot == 2 && !p2.use_touch &&
              r.lobby.state(0) == SeatState::Joining && r.map.assignment(0).use_touch,
          "the pad brings P2 back and the touch seats free P1 in the same tick");
}

void test_keys_seat_before_the_touch() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    r.keypress(::input::code::Space, true);
    r.touch(::input::code::TouchA, true);
    r.step();
    const ::input::PlayerAssign a = r.map.assignment(0);
    check(r.lobby.state(0) == SeatState::Joining && a.use_kbd_mouse && !a.use_touch,
          "a key and a touch button of one tick seat P1 on the keyboard");
}

void test_the_owner_pressing_its_touch_changes_nothing() {
    Rig r;
    add_touch(r, 0, ::input::code::TouchA);
    add_touch(r, 1, ::input::code::TouchA);
    check(r.lobby.join(0, {.use_touch = true}), "P1 on the touch");
    r.step();
    r.touch(::input::code::TouchA, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Present && r.map.assignment(0).use_touch &&
              r.lobby.state(1) == SeatState::Free && !r.map.assignment(1).use_touch,
          "the owner's own touch button neither moves the touch nor seats P2");
}

} // namespace

int main() {
    std::printf("framework: hotseat lobby, touch\n");
    test_a_touch_button_of_the_layout_seats_its_player();
    test_one_screen_holds_one_seat();
    test_a_pulled_pad_keeps_the_touch();
    test_the_lost_seat_takes_a_free_touch_first();
    test_a_touch_held_by_another_seat_brings_nobody_back();
    test_a_returning_pad_keeps_the_touch();
    test_a_pad_and_a_touch_in_one_tick_seat_two();
    test_keys_seat_before_the_touch();
    test_the_owner_pressing_its_touch_changes_nothing();
    return verdict("framework-lobby-touch");
}
