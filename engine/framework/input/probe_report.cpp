#include "probe_report.hpp"

#include <cstdio>
#include <string>

#include "rebind_session.hpp"
#include "source_names.hpp"

namespace framework::input {

void report_bindings(const PresetTable& table, uint32_t preset, const RebindStore& store) {
    std::printf("\n[probe] bindings of preset '%s':\n", table.preset_name(preset));
    for (uint32_t a = 0; a < table.action_count(preset); ++a) {
        std::printf("  %u) %-8s", a + 1, table.action_name(preset, a));
        for (uint32_t w = 0; w < table.action_binding_count(preset, a); ++w) {
            ::input::Source s;
            const bool have = effective_source(table, preset, store, a, w, s);
            const std::string name = have ? source_name(s) : std::string();
            std::printf(" [%u]=%-14s", w, name.empty() ? "none" : name.c_str());
        }
        std::printf("\n");
    }
    std::printf("[probe] keys: 1..%u rebind action - Tab alt slot - Enter/F apply - C cancel - "
                "S save - X reset all - Esc quit\n",
                table.action_count(preset));
}

void report_pads(::input::InputEngine& engine, ::input::GamepadSource* pad, PadRegistry& reg,
                 const PresetTable& table, bool* prev) {
    for (int s = 0; s < ::input::MAX_DEVICES; ++s) {
        const bool now = engine.device().pad_connected[s];
        if (now == prev[s]) continue;
        prev[s] = now;
        if (!now) {
            reg.disconnected(s);
            std::printf("\n[probe] pad %d DISCONNECTED -> profile falls back to '%s'\n", s,
                        reg.profile(s).name);
            continue;
        }
        const ::input::PadInfo info = pad != nullptr ? pad->pad_info(s) : ::input::PadInfo{};
        reg.connected(s, info, table);
        std::printf("\n[probe] pad %d CONNECTED vid=%04x pid=%04x name=\"%s\" -> profile '%s' "
                    "(deadzone %.2f, trigger %.2f)\n",
                    s, info.vid, info.pid, info.name, reg.profile(s).name,
                    reg.profile(s).stick.deadzone.to_double(),
                    reg.profile(s).trigger_threshold.to_double());
    }
}

void report_status(const PresetTable& table, uint32_t preset, const ::input::InputFrame& frame,
                   const PadRegistry& reg, uint32_t tick) {
    std::printf("\r[t%6u] ", tick);
    for (uint32_t a = 0; a < table.action_count(preset); ++a)
        std::printf("%s:%s ", table.action_name(preset, a),
                    frame.action_held(static_cast<int>(a)) ? "#" : ".");
    std::printf("move=(%+.2f,%+.2f) pad0='%s'   ", frame.axes[0].to_double(),
                frame.axes[1].to_double(), reg.profile(0).name);
    std::fflush(stdout);
}

} // namespace framework::input
