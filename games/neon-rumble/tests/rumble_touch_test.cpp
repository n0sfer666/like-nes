#include <cstdio>
#include <string>

#include "bundle_writer.hpp"
#include "codes.hpp"
#include "hash.hpp"
#include "platform_fs.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_touch.hpp"

namespace {
namespace tl = framework::touch;
namespace bb = framework::brawl::button;
using framework::input::SeatState;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("  FAIL: %s\n", why); } }
fix32 pt(int n) { return fix32::from_int(n); }

struct Rig {
    rumble::Hotseat hotseat;
    rumble::Touch touch;
    rumble::Players players{};
    std::vector<uint8_t> bytes;
    asset::BundleView bundle;
    uint32_t tick = 0;
    uint64_t seq = 1'000'000;

    bool load(tl::Layout layout) {
        std::vector<uint8_t> raw;
        std::string why;
        if (!tl::bake_layout(layout, raw, why)) return false;
        bytes = asset::write_bundle({{asset::fnv1a_str("touch"), asset::AssetType::Raw,
            asset::Codec::Raw, asset::Residency::Mmap, raw, static_cast<uint32_t>(raw.size())}});
        return bundle.open(bytes.data(), bytes.size(), false) && touch.load(bundle, why);
    }
    Rig(bool mobile = true) {
        check(hotseat.open(mobile), "actual game controls and lobby open");
        std::string text, why;
        tl::Layout layout;
        check(platform::read_text(RUMBLE_TEST_TOUCH, text) && tl::parse_layout(text, "touch.txt", layout, why) &&
              load(layout), "actual game's layout bakes, bundles and loads");
        touch.source().bind(&hotseat.engine());
        touch.source().set_view(pt(844), pt(390), {pt(44), {}, pt(44), pt(21)});
    }
    bool step() { return hotseat.tick(++tick, players); }
    void button(uint32_t index, bool down) {
        const auto b = touch.source().button(index);
        check(b.has_value(), "combat button exists");
        if (b) touch.source().pointer(100 + index, down ? tl::Phase::Down : tl::Phase::Up,
                                      b->center.x, b->center.y);
    }
    void pad(input::RawKind kind, uint16_t code = 0) {
        hotseat.engine().post({kind, input::DeviceKind::Gamepad, 0, code, 0, seq++});
    }
    void join() {
        button(0, true); check(step() && hotseat.touch_player() == 0, "first touch claims P1");
        button(0, false); check(step() && players[0].input.present, "touch P1 becomes present");
    }
};

void combat() {
    Rig r;
    check(r.hotseat.lobby().state(0) == SeatState::Free, "mobile starts without a keyboard player");
    const auto a = r.touch.source().button(0);
    check(a && a->center.x == pt(732) && a->center.y == pt(301) && a->radius == pt(30),
          "layout anchors in safe-area points");
    r.join();
    for (uint32_t b = 0; b < tl::BUTTONS; ++b) {
        r.button(b, true); check(r.step(), "button tick runs");
        const auto& c = r.players[0];
        if (b == 0) check(c.attack == rumble::Attack::Punch, "a maps to PUNCH");
        if (b == 1) check(c.attack == rumble::Attack::Kick, "b maps to KICK");
        if (b == 2) check((c.input.buttons & bb::JUMP) != 0, "c maps to JUMP");
        if (b == 3) check(c.attack == rumble::Attack::Grab && (c.input.buttons & bb::GRAB), "d maps to GRAB");
        if (b == 4) check((c.input.buttons & bb::BLOCK) != 0, "e maps to BLOCK");
        r.button(b, false); check(r.step(), "release tick runs");
    }
    r.button(4, true);
    r.touch.source().pointer(42, tl::Phase::Down, pt(180), pt(240));
    check(r.step(), "block with centered stick");
    r.touch.source().pointer(42, tl::Phase::Move, pt(228), pt(240));
    check(r.step() && r.players[0].input.move.move_x == pt(1) &&
          (r.players[0].input.buttons & bb::DODGE), "real command_of turns BLOCK + stick flick into DODGE");
    check(r.touch.state().find("stick=42") != std::string::npos &&
          r.touch.state().find("e=104") != std::string::npos, "probe reports independent owners");
    r.touch.source().set_view(pt(844), pt(390), {pt(68), {}, pt(20), pt(21)});
    check(r.step() && r.players[0].input.move.move_x == pt(0) && r.players[0].input.buttons == 0 &&
          r.touch.source().idle(), "safe-only change releases stick and buttons");
    check(r.touch.source().button(0)->center.x == pt(756), "button follows changed cutout side");
}

void reclaim() {
    Rig r;
    r.pad(input::RawKind::DeviceConnected);
    r.pad(input::RawKind::PadButtonDown, input::code::PadA);
    check(r.step() && r.hotseat.lobby().state(0) == SeatState::Joining, "first pad claims P1");
    r.pad(input::RawKind::PadButtonUp, input::code::PadA); check(r.step(), "pad becomes present");
    r.pad(input::RawKind::DeviceDisconnected);
    check(!r.step() && r.hotseat.lobby().lost(0), "lost pad pauses the game");
    r.button(0, true);
    check(!r.step() && r.hotseat.lobby().state(0) == SeatState::Resuming && r.hotseat.touch_player() == 0,
          "touch reclaims Lost before the free P2 seat");
    r.button(0, false); check(r.step(), "reclaimed player resumes next tick");
    r.pad(input::RawKind::DeviceConnected);
    r.pad(input::RawKind::PadButtonDown, input::code::PadA); check(r.step(), "pad claims P2 beside touch");
    r.pad(input::RawKind::PadButtonUp, input::code::PadA); check(r.step(), "P2 becomes present");
    r.pad(input::RawKind::DeviceDisconnected); check(!r.step(), "P2 lost");
    r.button(0, true);
    check(!r.step() && r.hotseat.lobby().lost(1) && r.hotseat.touch_player() == 0,
          "one touch screen cannot resume a second seat");
}

void refusals() {
    Rig r;
    tl::Layout short_layout;
    short_layout.stick_radius = pt(48);
    short_layout.button_count = 1;
    short_layout.buttons[0] = {0, tl::Anchor::BottomRight, pt(68), pt(68), pt(30)};
    check(!r.load(short_layout) && !r.touch.source().button(0), "valid but incomplete layout refused and reset");
    std::string why;
    asset::BundleView unopened;
    check(!r.touch.load(unopened, why) && !why.empty(), "unopened bundle refused");
    Rig desktop(false);
    desktop.button(0, true);
    check(desktop.step() && desktop.hotseat.touch_player() == -1 &&
          desktop.players[0].attack == rumble::Attack::None, "desktop keyboard mode does not consume touch");
}

bool flick_at(int delay, int32_t raw, bool block = true) {
    Rig r;
    r.join();
    if (block) r.button(4, true);
    r.touch.source().pointer(42, tl::Phase::Down, pt(180), pt(240));
    check(r.step(), "flick starts centered");
    for (int i = 0; i < delay; ++i) {
        r.touch.source().pointer(42, tl::Phase::Move, pt(216), pt(240));
        check(r.step(), "sub-threshold drift tick");
    }
    const fix32 dx = fix32::from_raw(raw) * pt(48);
    r.touch.source().pointer(42, tl::Phase::Move, pt(180) + dx, pt(240));
    check(r.step(), "flick final tick");
    return (r.players[0].input.buttons & bb::DODGE) != 0;
}

void flick_boundary() {
    // 54067 даёт ровно .75 после deadzone .3: (54067 - 19660) / (65536 - 19660).
    check(flick_at(2, 54067), "game threshold .75 and window K=3 include their boundary");
    check(!flick_at(3, 54067), "game window excludes K+1 drift ticks");
    check(!flick_at(0, 54066), "one raw unit below game threshold does not dodge");
    check(flick_at(0, -54067), "negative stick flick also dodges");
    check(!flick_at(0, 54067, false), "stick flick without BLOCK does not dodge");
}

} // namespace

int main() {
    combat(); reclaim(); refusals(); flick_boundary();
    std::puts(fails ? "rumble-touch: FAIL" : "rumble-touch: PASS");
    return fails ? 1 : 0;
}
