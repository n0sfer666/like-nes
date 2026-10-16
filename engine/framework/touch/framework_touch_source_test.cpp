#include <cstdio>
#include <optional>

#include "action_map.hpp"
#include "codes.hpp"
#include "input_engine.hpp"
#include "touch_source.hpp"

namespace {

int fails = 0;
int cases = 0;

void check(bool ok, const char* what) {
    ++cases;
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::touch;
namespace c = input::code;

fix32 pt(int32_t v) { return fix32::from_int(v); }

Layout layout() {
    Layout l;
    l.stick_radius = pt(50);
    l.buttons[0] = {0, Anchor::BottomRight, pt(72), pt(64), pt(40)};
    l.buttons[1] = {1, Anchor::TopLeft, pt(30), pt(30), pt(20)};
    l.button_count = 2;
    return l;
}

const Insets NOTCH{pt(44), pt(0), pt(44), pt(21)};

struct Rig {
    input::ActionMap map;
    input::InputEngine engine{map};
    TouchSource touch;
    bool ready = false;

    Rig() {
        touch.bind(&engine);
        ready = touch.set_layout(layout());
        touch.set_view(pt(800), pt(400), NOTCH);
    }
    void at(intptr_t id, Phase phase, int32_t x, int32_t y) { touch.pointer(id, phase, pt(x), pt(y)); }
    bool down(int code) {
        engine.drain();
        return engine.device().touch_down(code);
    }
    fix32 axis(int code) {
        engine.drain();
        return engine.device().touch_axis(code);
    }
};

bool owned_by(const TouchSource& t, Control c, intptr_t id) {
    const std::optional<intptr_t> o = t.owner(c);
    return o && *o == id;
}

void test_geometry() {
    Rig r;
    check(r.ready, "a valid layout is taken");
    const std::optional<Circle> a = r.touch.button(0);
    check(a && a->center.x == pt(684) && a->center.y == pt(315) && a->radius == pt(40),
          "bottom-right button sits inside the safe area corner");
    const std::optional<Circle> b = r.touch.button(1);
    check(b && b->center.x == pt(74) && b->center.y == pt(30), "top-left button counts from the left inset");
    check(!r.touch.button(2), "a button outside the layout has no circle");
    r.touch.set_view(pt(800), pt(400), Insets{});
    check(r.touch.button(0)->center.x == pt(728), "without a notch the button moves to the screen corner");
}

void test_buttons() {
    Rig r;
    r.at(7, Phase::Down, 684, 315);
    check(r.down(c::TouchA) && owned_by(r.touch, Control::A, 7), "a finger on button a holds it");
    r.at(7, Phase::Up, 684, 315);
    check(!r.down(c::TouchA) && r.touch.idle(), "lifting the finger releases a");
    r.at(8, Phase::Down, 724, 315);
    check(r.down(c::TouchA), "the rim of the button is still the button");
    r.at(8, Phase::Up, 724, 315);
    r.at(9, Phase::Down, 725, 315);
    check(!r.down(c::TouchA) && r.touch.idle(), "a point past the rim on the right half holds nothing");
    r.at(10, Phase::Down, 74, 30);
    check(r.down(c::TouchB) && !r.touch.owner(Control::Stick), "a button on the left half wins over the stick");
    r.at(11, Phase::Down, 74, 30);
    check(owned_by(r.touch, Control::B, 10) && !r.touch.owner(Control::Stick),
          "a second finger on a held left button does not fall through to the stick");
}

void test_floating_stick() {
    Rig r;
    r.at(1, Phase::Down, 200, 200);
    check(owned_by(r.touch, Control::Stick, 1) && r.axis(c::StickX) == fix32{}, "the stick lands under the finger");
    r.at(1, Phase::Move, 225, 200);
    check(r.axis(c::StickX) == fix32::from_raw(32768), "half the radius is half the tilt");
    r.at(1, Phase::Move, 200, 150);
    check(r.axis(c::StickY) == pt(-1) && r.axis(c::StickX) == fix32{}, "up the screen is negative y");
    r.at(1, Phase::Move, 300, 200);
    check(r.axis(c::StickX) == pt(1) && r.touch.stick_base()->center.x == pt(250),
          "a finger past the radius drags the base after it");
    r.at(1, Phase::Move, 240, 200);
    check(r.axis(c::StickX) == fix32::from_raw(-13107), "coming back tilts the other way at once");
    check(r.touch.stick_knob()->x == pt(240), "the knob is the finger");
    r.at(1, Phase::Move, 400, 400);
    const Vec2 v{r.axis(c::StickX), r.axis(c::StickY)};
    check(!(pt(1) < length(v)) && fix32::from_raw(65000) < length(v), "a diagonal tilt stays in the unit circle");
    r.at(1, Phase::Up, 400, 400);
    check(r.axis(c::StickX) == fix32{} && r.axis(c::StickY) == fix32{} && r.touch.idle(),
          "lifting the stick finger centres both axes");
    r.at(2, Phase::Down, 500, 200);
    check(r.touch.idle(), "the right half outside the buttons is no stick");
}

void test_two_fingers() {
    Rig r;
    r.at(1, Phase::Down, 200, 200);
    r.at(2, Phase::Down, 684, 315);
    r.at(1, Phase::Move, 250, 200);
    check(r.down(c::TouchA) && r.axis(c::StickX) == pt(1), "stick and button held at once");
    r.at(3, Phase::Down, 100, 300);
    check(owned_by(r.touch, Control::Stick, 1), "a second finger on the left half does not take the stick");
    r.at(4, Phase::Down, 684, 315);
    r.at(4, Phase::Up, 684, 315);
    check(r.down(c::TouchA) && owned_by(r.touch, Control::A, 2), "a second finger on a held button lifts nothing");
    r.at(2, Phase::Move, 100, 100);
    check(r.down(c::TouchA) && r.axis(c::StickX) == pt(1), "a button finger moving does not move the stick");
    r.at(2, Phase::Up, 100, 100);
    check(!r.down(c::TouchA) && owned_by(r.touch, Control::Stick, 1), "lifting the button finger keeps the stick");
    r.at(1, Phase::Down, 684, 315);
    check(r.down(c::TouchA) && owned_by(r.touch, Control::A, 1) && !r.touch.owner(Control::Stick) &&
              r.axis(c::StickX) == fix32{},
          "a down of a finger whose up was lost lets go first and starts over");
}

void test_cancel() {
    Rig r;
    r.at(1, Phase::Down, 200, 200);
    r.at(1, Phase::Move, 250, 200);
    r.at(2, Phase::Down, 684, 315);
    r.touch.cancel();
    check(r.touch.idle() && !r.down(c::TouchA) && r.axis(c::StickX) == fix32{}, "cancel lets go of everything");
    r.at(1, Phase::Move, 300, 200);
    check(r.axis(c::StickX) == fix32{}, "a finger cancelled by the system moves nothing");
    r.at(5, Phase::Down, 684, 315);
    r.touch.set_layout(layout());
    check(r.touch.idle() && !r.down(c::TouchA), "a new layout lets go of the old buttons");
    r.at(6, Phase::Down, 684, 315);
    r.touch.set_view(pt(800), pt(400), NOTCH);
    check(owned_by(r.touch, Control::A, 6), "the same view keeps the fingers");
    r.touch.set_view(pt(400), pt(800), NOTCH);
    check(r.touch.idle() && !r.down(c::TouchA), "a rotated view lets go of the fingers");
    TouchSource unbound;
    unbound.set_layout(layout());
    unbound.set_view(pt(800), pt(400), NOTCH);
    unbound.pointer(1, Phase::Down, pt(684), pt(315));
    check(owned_by(unbound, Control::A, 1), "a source without an engine still tracks fingers");
}

void test_bad_layout() {
    Rig r;
    Layout bad = layout();
    bad.buttons[1].button = 5;
    check(!r.touch.set_layout(bad) && !r.touch.button(0), "a button number past e is refused and leaves no layout");
    r.at(1, Phase::Down, 200, 200);
    r.at(1, Phase::Move, 200, 200);
    check(r.touch.idle() && r.axis(c::StickX) == fix32{}, "without a layout the stick does not land");
    TouchSource bare;
    bare.set_view(pt(800), pt(400), NOTCH);
    bare.pointer(1, Phase::Down, pt(200), pt(200));
    check(bare.idle(), "a source never given a layout has no stick");
}

} // namespace

int main() {
    std::printf("touch source: screen fingers to TouchButton and TouchAxis events\n");
    test_geometry();
    test_buttons();
    test_floating_stick();
    test_two_fingers();
    test_cancel();
    test_bad_layout();
    std::printf("framework-touch-source: %s - %d cases\n", fails == 0 ? "PASS" : "FAIL", cases);
    return fails == 0 ? 0 : 1;
}
