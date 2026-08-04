#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "codes.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"
#include "rebind_session.hpp"
#include "rebind_store.hpp"

// Гейт 4 спеки #14: перебинд «нажми клавишу» доходит до действия, конфликт виден и разрешается,
// а профиль игрока переживает перезапуск и не превращает битый файл в половину раскладки.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* MANIFEST = R"(
preset | default
action | fire | key:space | pad:south
action | jump | key:s
)";

::input::RawEvent key_down(uint16_t code) {
    ::input::RawEvent e;
    e.kind = ::input::RawKind::KeyDown;
    e.code = code;
    return e;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;
    namespace c = ::input::code;

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(MANIFEST, blob, err)) {
        std::printf("  FAIL: bake failed at line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }
    PresetTable t;
    check(t.open(blob.data(), blob.size()), "the table opens");
    const uint32_t P = 0;
    const int fire = t.find_action(P, "fire");
    const int jump = t.find_action(P, "jump");
    check(fire == 0 && jump == 1, "actions keep their manifest order");

    RebindStore store;
    RebindSession s;

    // Сессия слушает СЫРОЙ поток: отпускание клавиши и оси не назначаются сами.
    s.begin(fire, 0);
    ::input::RawEvent up;
    up.kind = ::input::RawKind::KeyUp;
    up.code = c::Enter;
    check(!s.feed(up), "a key-up does not become a binding");
    check(s.feed(key_down(c::Enter)) && s.captured(), "a key-down is captured");
    RebindConflict conflict;
    check(s.commit(t, P, store, false, &conflict) && conflict.action < 0,
          "a free source commits without a conflict");
    check(!s.active(), "a committed session closes itself");

    ::input::Source got;
    check(effective_source(t, P, store, static_cast<uint32_t>(fire), 0, got) &&
              got.kind == ::input::SourceKind::Key && got.code == c::Enter,
          "the override wins over the preset");
    check(effective_source(t, P, store, static_cast<uint32_t>(fire), 1, got) &&
              got.kind == ::input::SourceKind::PadButton,
          "the untouched alternative still comes from the preset");

    // Конфликт: занятый источник не применяется молча.
    s.begin(jump, 0);
    check(s.feed(key_down(c::Enter)), "the same key is captured for another action");
    check(!s.commit(t, P, store, false, &conflict) && conflict.action == fire && conflict.which == 0,
          "an occupied source is refused and names its owner");
    check(effective_source(t, P, store, static_cast<uint32_t>(jump), 0, got) && got.code == c::S,
          "the refused rebind changed nothing");
    check(s.captured(), "the refused session stays open for the player's decision");
    check(s.commit(t, P, store, true, &conflict) && conflict.action == fire,
          "force takes the source from the previous owner");
    check(effective_source(t, P, store, static_cast<uint32_t>(fire), 0, got) &&
              got.kind == ::input::SourceKind::None,
          "the previous owner loses that binding instead of sharing it");
    check(effective_source(t, P, store, static_cast<uint32_t>(jump), 0, got) &&
              got.code == c::Enter,
          "the new owner got the source");

    // Перебинды доезжают до ActionMap поверх залитого пресета.
    ::input::ActionMap map;
    check(t.bind(P, map), "the preset binds");
    store.apply(t, P, map);
    ::input::PlayerAssign assign;
    assign.use_kbd_mouse = true;
    map.assign_player(0, assign);
    ::input::DeviceState d;
    d.keys[c::Enter / 64] |= (1ull << (c::Enter % 64));
    const ::input::InputFrame f = map.resolve(d, 0, 0, 0);
    check(f.action_held(static_cast<uint32_t>(jump)), "enter now fires the rebound action");
    check(!f.action_held(static_cast<uint32_t>(fire)), "and no longer fires the old one");

    // Сохранение: накладка переживает перезапуск и хранится по ИМЕНИ действия.
    const std::string path = std::string(platform::exe_dir().empty() ? "." : platform::exe_dir()) +
                             "/framework_rebind_test.profile";
    check(store.save(path, "default"), "the profile is written to disk");
    RebindStore back;
    std::string preset_name;
    check(back.load(path, preset_name) && preset_name == "default",
          "the profile loads and names its preset");
    ::input::Source a, b;
    check(back.get("jump", 0, a) && a.code == c::Enter, "the rebound source survived the trip");
    check(back.get("fire", 0, b) && b.kind == ::input::SourceKind::None,
          "the cleared binding survived too instead of coming back from the preset");

    // Битый файл — чистый пресет, а не половина раскладки.
    RebindStore broken;
    std::string ignored;
    check(!broken.parse("preset | default\nbind | jump | 0 | key:enter\nbind | fire | x | ???\n",
                        ignored) &&
              broken.empty(),
          "a corrupt profile is refused whole, not applied halfway");
    check(!broken.parse("bind | jump | 0 | key:enter\n", ignored),
          "a profile without a preset name is refused");

    // Сброс к пресету — удаление накладки.
    store.reset("jump");
    check(effective_source(t, P, store, static_cast<uint32_t>(jump), 0, got) && got.code == c::S,
          "resetting one action returns it to the preset");
    store.reset_all();
    check(store.empty(), "resetting everything leaves no overrides");

    platform::remove_file(path);
    const bool pass = (fails == 0);
    std::printf("framework-rebind: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
