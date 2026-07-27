#include <cstdio>
#include <cstring>
#include <vector>

#include "codes.hpp"
#include "pad_registry.hpp"
#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"

// Гейты 2 и 5 спеки #14: профиль выбирается по паспорту устройства (VID/PID, при нулях — имя),
// неизвестный пад играет по generic, а подключение и отключение слота не оставляют следов.
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
action | jump | key:space | pad:south

pad | Microsoft Xbox      | 0x045e | -      | Xbox      | xbox        | 0.18 | 0.12
pad | Sony DualSense      | 0x054c | 0x0ce6 | DualSense | playstation | 0.10 | 0.10
pad | Nintendo Switch Pro | 0x057e | -      | Nintendo  | nintendo    | 0.15 | 0.12
)";

::input::PadInfo pad(uint16_t vid, uint16_t pid, const char* name) {
    ::input::PadInfo i;
    i.vid = vid;
    i.pid = pid;
    std::strncpy(i.name, name, sizeof(i.name) - 1);
    return i;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(MANIFEST, blob, err)) {
        std::printf("  FAIL: bake failed at line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }
    PresetTable t;
    check(t.open(blob.data(), blob.size()), "the table with pad rows opens");

    // VID/PID — первичный канал: имя устройства при этом не обязано ни на что походить.
    const PadProfile xbox = t.profile_for(pad(0x045e, 0x02ea, "Controller (XBOX 360 For Windows)"));
    check(std::strcmp(xbox.name, "Microsoft Xbox") == 0, "the Xbox vendor id selects its profile");
    check(xbox.stick.deadzone == fix32::from_float(0.18), "the profile carries its own deadzone");

    // Строка с конкретным PID не должна ловить чужой пад того же вендора.
    const PadProfile ds = t.profile_for(pad(0x054c, 0x0ce6, "Wireless Controller"));
    check(std::strcmp(ds.name, "Sony DualSense") == 0, "vendor and product together select the profile");
    check(ds.labels == PadLabels::Playstation, "the label set survives the bake");
    check(std::strcmp(t.profile_for(pad(0x054c, 0x1234, "Wireless Controller")).name, "generic") == 0,
          "another product of the same vendor does not inherit that profile");

    // Нули VID/PID — это XInput и GameController.framework, то есть две ОС из трёх. Имя обязано
    // решать в одиночку, и регистр в нём не наш.
    check(std::strcmp(t.profile_for(pad(0, 0, "Nintendo Switch Pro Controller")).name,
                      "Nintendo Switch Pro") == 0,
          "with no ids the device name selects the profile");
    check(std::strcmp(t.profile_for(pad(0, 0, "DUALSENSE wireless")).name, "Sony DualSense") == 0,
          "the name match ignores case");
    check(std::strcmp(t.profile_for(pad(0, 0, "Generic USB Joystick")).name, "generic") == 0,
          "an unknown pad falls back to the generic profile instead of a guess");
    check(t.profile_for(pad(0, 0, "Generic USB Joystick")).labels == PadLabels::Xbox,
          "the generic profile is the engine's own normalized model");

    // Гейт 2: один и тот же сценарий даёт ОДНИ И ТЕ ЖЕ действия на любом профиле. Коды кнопок
    // позиционные у всех трёх бэкендов, поэтому профиль не смеет их переставлять — меняются
    // только надписи для подсказок.
    namespace c = ::input::code;
    ::input::ActionMap map;
    check(t.bind(0, map), "the preset binds");
    ::input::PlayerAssign assign;
    assign.pad_slot = 0;
    map.assign_player(0, assign);
    ::input::DeviceState d;
    d.pad_connected[0] = true;
    d.pad_btns[0] |= (1u << c::PadA);
    check(map.resolve(d, 0, 0, 0).action_held(0), "the south button fires jump");
    check(std::strcmp(button_label(xbox, c::PadA), "A") == 0, "on Xbox the south button reads A");
    check(std::strcmp(button_label(t.profile_for(pad(0x057e, 0, "Pro Controller")), c::PadA), "B") == 0,
          "on Nintendo the same position reads B");
    check(std::strcmp(button_label(ds, c::PadA), "Cross") == 0,
          "on PlayStation the same position reads Cross");

    // Гейт 5: hot-plug. Слот получает профиль на подключении и теряет его на отключении, а
    // следующий владелец слота не наследует чужую мёртвую зону.
    PadRegistry reg;
    check(!reg.active(0), "a slot starts empty");
    check(std::strcmp(reg.profile(0).name, "generic") == 0,
          "an empty slot answers with the generic profile, not with garbage");
    reg.connected(0, pad(0x057e, 0, "Pro Controller"), t);
    check(reg.active(0) && std::strcmp(reg.profile(0).name, "Nintendo Switch Pro") == 0,
          "connecting resolves the profile once");
    reg.disconnected(0);
    check(!reg.active(0) && std::strcmp(reg.profile(0).name, "generic") == 0,
          "disconnecting clears the slot");
    reg.connected(0, pad(0x054c, 0x0ce6, "Wireless Controller"), t);
    check(reg.profile(0).stick.deadzone == fix32::from_float(0.10),
          "the next owner of the slot gets its own deadzone");
    reg.connected(9, pad(0x045e, 0, "Xbox"), t);
    check(!reg.active(9), "a slot outside the device range is refused, not written past the array");

    // Строка профиля, которую невозможно выбрать, — ошибка манифеста, а не мёртвый вес.
    std::vector<uint8_t> ignored;
    check(!bake_presets("preset | p\naction | a | key:space\npad | x | - | - | - | xbox | 0.1 | 0.1\n",
                        ignored, err) && err.line == 3,
          "a pad row with neither ids nor a name match is refused");
    check(!bake_presets("preset | p\naction | a | key:space\npad | x | 0x045e | - | - | martian | 0.1 | 0.1\n",
                        ignored, err) && err.line == 3,
          "an unknown label set is refused");

    const bool pass = (fails == 0);
    std::printf("framework-pad: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
