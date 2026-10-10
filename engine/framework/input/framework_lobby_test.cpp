#include "framework_lobby_rig.hpp"

// Спека #25 В4в: вход нажатием свободного устройства, present со следующего тика, выход и границы.
namespace {

using namespace framework::input;
using namespace framework::input::test;

void test_a_free_pad_joins_from_the_next_tick() {
    Rig r;
    check(r.lobby.join(0, {-1, true}), "P1 sits at the keyboard");
    r.plug(0);
    r.step();
    check(r.lobby.state(1) == SeatState::Free, "a pad that only connected takes no seat");
    r.button(0, ::input::code::PadX, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Joining && !r.lobby.present(1), "the joining press is not presence yet");
    r.step();
    check(r.lobby.present(1) && r.map.assignment(1).pad_slot == 0, "P2 is present on pad 0 from the next tick");
    check(!r.engine.frame(1).action_pressed(PUNCH) && r.engine.frame(1).action_held(PUNCH),
          "the press that joined is a held level, not a punch on the first present tick");
}

void test_a_button_held_from_the_menu_is_not_a_join() {
    Rig r(KEY_KP1, false);
    r.plug(0);
    r.button(0, ::input::code::PadA, true);
    r.keypress(KEY_KP1, true);
    r.step();
    r.step();
    check(r.lobby.state(0) == SeatState::Free && r.lobby.state(1) == SeatState::Free,
          "a pad button and a key held before the lobby's first update seat nobody");
    r.button(0, ::input::code::PadA, false);
    r.step();
    r.button(0, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Joining, "pressing it again does");
}

::input::ActionLayout keys_only() {
    ::input::ActionLayout l;
    l.bind(PUNCH, key(KEY_J));
    return l;
}

void test_an_owned_pad_and_a_full_lobby_take_no_seat() {
    Rig r;
    r.button(4, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Free, "a button from a pad that never connected seats nobody");
    r.plug(0);
    r.plug(1);
    r.plug(2);
    check(r.map.set_layout(0, keys_only()) && r.lobby.join(0, {0, false}), "P1 owns pad 0 and binds no pad button");
    check(!r.lobby.join(1, {0, false}), "a join onto P1's pad is refused even with no button shared");
    r.button(0, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Free, "a press on an owned pad seats nobody, even with no button shared");
    r.button(1, ::input::code::PadA, true);
    r.button(2, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(1) == SeatState::Joining && r.map.assignment(1).pad_slot == 1,
          "the lower free pad takes the one free seat");
    check(r.map.assignment(0).pad_slot == 0, "the full lobby leaves the third pad unseated and P1 untouched");
}

void test_a_keyboard_half_joins_its_own_player() {
    const uint16_t keys[] = {KEY_KP1, KEY_RIGHT, KEY_LEFT};
    const char* what[] = {"a button key of P2's half seats P2 on the keyboard",
                          "the positive arrow of P2's axis seats P2", "the negative arrow of P2's axis seats P2"};
    for (int k = 0; k < 3; ++k) {
        Rig r;
        check(r.lobby.join(0, {-1, true}), "P1 sits at the keyboard");
        r.keypress(KEY_J, true);
        r.step();
        check(r.lobby.state(1) == SeatState::Free, "P1's key seats nobody");
        r.keypress(keys[k], true);
        r.step();
        check(r.lobby.state(1) == SeatState::Joining && r.map.assignment(1).use_kbd_mouse, what[k]);
        r.lobby.leave(1);
        r.step();
        check(r.lobby.state(1) == SeatState::Free, "a key still held after leaving does not seat P2 again");
    }

    Rig shared(KEY_J);
    check(shared.lobby.join(0, {-1, true}), "P1 sits at the keyboard");
    shared.keypress(KEY_J, true);
    shared.step();
    shared.step();
    check(shared.lobby.state(1) == SeatState::Free && shared.map.assignment(0).use_kbd_mouse,
          "a half that shares a key with P1 is refused, not doubled");
}

void test_leave_frees_the_seat_and_the_pad() {
    Rig r;
    r.plug(0);
    check(r.lobby.join(1, {0, false}), "P2 on pad 0");
    r.lobby.leave(1);
    check(!r.lobby.present(1) && r.map.assignment(1).pad_slot == -1, "leaving drops presence and the pad");
    r.button(0, ::input::code::PadA, true);
    r.step();
    check(r.lobby.state(0) == SeatState::Joining && r.map.assignment(0).pad_slot == 0,
          "the freed pad seats the lowest free player");
    check(!r.lobby.join(5, {-1, true}) && !r.lobby.join(0, {-1, true}), "no join out of range or into a taken seat");
}

void test_out_of_range_players_touch_nothing() {
    ::input::ActionMap map;
    check(set_p1_layout(map, 0) && map.assign_player(0, {0, true}), "P1 has a layout and a pad");
    Lobby wide{map, 9};
    check(wide.seats() == ::input::MAX_PLAYERS && wide.join(3, {-1, true}) && !wide.join(4, {2, false}),
          "nine seats clamp to MAX_PLAYERS");
    Lobby none{map, -1};
    check(none.seats() == 0 && !none.join(0, {-1, true}), "negative seats clamp to none");
    check(!wide.join(2, {::input::MAX_DEVICES, false}) && !wide.join(2, {-2, false}) &&
              wide.state(2) == SeatState::Free,
          "a pad slot that cannot exist is refused, not a pause for a device that never comes");
    wide.leave(-1);
    wide.leave(7);
    check(wide.present(3) && wide.state(7) == SeatState::Free, "leaving out of range changes nobody");
    check(map.layout(-1).buttons().empty() && map.layout(4).axes().empty(), "a layout out of range is empty");
    check(map.assignment(4).pad_slot == -1 && !map.assignment(-1).use_kbd_mouse,
          "an assignment out of range is nobody's");
}

} // namespace

int main() {
    std::printf("framework: hotseat lobby\n");
    test_a_free_pad_joins_from_the_next_tick();
    test_a_button_held_from_the_menu_is_not_a_join();
    test_an_owned_pad_and_a_full_lobby_take_no_seat();
    test_a_keyboard_half_joins_its_own_player();
    test_leave_frees_the_seat_and_the_pad();
    test_out_of_range_players_touch_nothing();
    return verdict("framework-lobby");
}
