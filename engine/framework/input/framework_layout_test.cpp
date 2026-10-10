#include <cstdio>
#include <string>
#include <vector>

#include "codes.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "player_layout.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"
#include "profile_file.hpp"
#include "rebind_store.hpp"
#include "source_names.hpp"

// Спека #25 В4б: две раскладки на одной клавиатуре, у каждого игрока своя накладка и свой файл,
// и накладка, забравшая клавишу другого игрока, не встаёт, а называется текстом.
namespace {

using namespace framework::input;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Раскладки владельца для Neon Rumble: P1 — как в прототипе без стрелок, P2 — стрелки и нампад.
const char* MANIFEST = R"(
preset | p1
action | jump  | key:space | key:k
action | punch | key:j
action | cross | key:u
action | kick  | key:l
action | grab  | key:h
action | block | key:i
action | dodge | key:o
axis   | move_x | key:d | key:a
axis   | move_z | key:s | key:w

preset | p2
action | jump  | key:kp0
action | punch | key:kp1
action | cross | key:kp4
action | kick  | key:kp2
action | grab  | key:kp5
action | block | key:kp3
action | dodge | key:kp6
axis   | move_x | key:right | key:left
axis   | move_z | key:down  | key:up
)";

enum : uint16_t { KEY_F = 70, KEY_J = 74, KP_1 = 321, KP_9 = 329 };

::input::DeviceState key_down(uint16_t code) {
    ::input::DeviceState d;
    d.keys[code / 64] |= (1ull << (code % 64));
    return d;
}

bool punches(const ::input::ActionMap& map, int player, uint16_t key, int punch) {
    return map.resolve(key_down(key), player, 0, 0).action_held(static_cast<uint32_t>(punch));
}

bool kp_names_round_trip() {
    for (uint16_t code = 320; code <= 329; ++code) {
        const ::input::Source src{::input::SourceKind::Key, code, 1};
        ::input::Source back;
        const std::string name = source_name(src);
        if (name != "key:kp" + std::to_string(code - 320) || !parse_source(name, back) || back.code != code)
            return false;
    }
    ::input::Source ignored;
    return !parse_source("key:kp", ignored) && !parse_source("key:kpa", ignored) &&
           !parse_source("key:kp10", ignored);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(MANIFEST, blob, err)) {
        std::printf("  FAIL: bake failed at line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }
    PresetTable t;
    check(t.open(blob.data(), blob.size()), "the table opens");
    const int p1 = t.find_preset("p1"), p2 = t.find_preset("p2");
    const int punch = t.find_action(static_cast<uint32_t>(p2), "punch");
    check(p1 >= 0 && p2 >= 0 && punch == t.find_action(static_cast<uint32_t>(p1), "punch"),
          "both presets bake with the same action order");
    check(kp_names_round_trip(), "numpad keys are named kp0..kp9 both ways");

    ::input::ActionMap map;
    ::input::PlayerAssign kbd;
    kbd.use_kbd_mouse = true;
    RebindStore s1, s2;
    std::string error;
    check(build_layout(t, static_cast<uint32_t>(p1), s1, map, 0, error) &&
              build_layout(t, static_cast<uint32_t>(p2), s2, map, 1, error) && map.assign_player(0, kbd) &&
              map.assign_player(1, kbd),
          "two layouts share one keyboard");
    check(punches(map, 0, KEY_J, punch) && !punches(map, 1, KEY_J, punch) && punches(map, 1, KP_1, punch) &&
              !punches(map, 0, KP_1, punch),
          "each player's punch is their own key");

    s2.set("punch", 0, {::input::SourceKind::Key, KP_9, 1});
    check(build_layout(t, static_cast<uint32_t>(p2), s2, map, 1, error) && s1.empty(),
          "a P2 rebind goes into P2's store only");
    check(punches(map, 1, KP_9, punch) && !punches(map, 1, KP_1, punch) && punches(map, 0, KEY_J, punch),
          "the P2 rebind moves P2's punch and leaves P1's");

    RebindStore taken = s2;
    taken.set("punch", 0, {::input::SourceKind::Key, KEY_J, 1});
    check(!build_layout(t, static_cast<uint32_t>(p2), taken, map, 1, error) &&
              error == "player 2 and player 1 share key:j",
          "a P2 overlay on P1's key is refused by name");
    check(punches(map, 1, KP_9, punch) && !punches(map, 1, KEY_J, punch) && punches(map, 0, KEY_J, punch),
          "the refused overlay left the map as it was");
    check(!build_layout(t, static_cast<uint32_t>(p2), s2, map, ::input::MAX_PLAYERS, error) &&
              error == "player index 4 is out of range",
          "a player out of range is refused with a reason");
    check(!build_layout(t, 99, s2, map, 1, error) && error == "preset 99 does not bind" && punches(map, 1, KP_9, punch),
          "a preset that does not bind is refused and leaves the map");
    const ::input::Source pad_a{::input::SourceKind::PadButton, ::input::code::PadA, 1};
    check(shared_input_text({1, 0, pad_a, 0}) == "player 2 and player 1 share " + source_name(pad_a) + " on pad 1",
          "a shared pad input names the pad from one");

    check(profile_file("controls.txt", 0) == "controls.txt" && profile_file("controls.txt", 1) == "controls-p2.txt",
          "P1 keeps controls.txt, P2 gets controls-p2.txt");
    check(profile_file("save.d/controls", 3) == "save.d/controls-p4" && profile_file("controls.txt", -1).empty() &&
              profile_file("controls.txt", ::input::MAX_PLAYERS).empty() &&
              profile_file(".controls", 1) == ".controls-p2" &&
              profile_file("C:\\saves.d\\controls", 1) == "C:\\saves.d\\controls-p2",
          "the suffix goes before the file's own extension, and no file for a player out of range");

    const std::string dir = platform::exe_dir().empty() ? "." : platform::exe_dir();
    const std::string base = dir + "/framework_layout_test.txt";
    const std::string f1 = profile_file(base, 0), f2 = profile_file(base, 1);
    s1.set("kick", 0, {::input::SourceKind::Key, KEY_F, 1});
    check(s1.save(f1, "p1") && s2.save(f2, "p2"), "each player's profile is written to its own file");
    RebindStore b1, b2;
    std::string n1, n2;
    ::input::Source k, p;
    check(b1.load(f1, n1) && b2.load(f2, n2) && n1 == "p1" && n2 == "p2", "both profiles load with their presets");
    check(b1.get("kick", 0, k) && k.code == KEY_F && !b1.get("punch", 0, p) && b2.get("punch", 0, p) &&
              p.code == KP_9 && !b2.get("kick", 0, k),
          "neither profile carries the other's rebinds");
    platform::remove_file(f1);
    platform::remove_file(f2);

    const bool pass = (fails == 0);
    std::printf("framework-layout: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
