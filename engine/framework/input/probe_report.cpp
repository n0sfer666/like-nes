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
    // Enter и F названы РАЗНЫМ: они и делают разное. «Enter/F apply» читалось как «две клавиши,
    // одно действие», и владелец жал F — то есть форс, — из-за чего перебинд в занятый источник
    // молча забирал слот, ни разу не показав отказ. Шаг гейта про конфликт при этом выглядел
    // пройденным.
    std::printf("[probe] keys: 1..%u rebind action - Tab alt slot - Enter apply (refuses on "
                "conflict) - F force - C cancel - S save - X reset all - Esc quit\n",
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

void report_cold_start(::input::InputEngine& engine, const ::input::GamepadSource* pad) {
    int seen = 0;
    for (int s = 0; s < ::input::MAX_DEVICES; ++s)
        seen += engine.device().pad_connected[s] ? 1 : 0;
    if (seen > 0) {
        std::printf("[probe] cold-start scan: %d pad(s) already connected - see the lines above\n",
                    seen);
        return;
    }
    // Текст английский намеренно: консоль Windows не UTF-8, и русский из этого потока приезжает
    // кракозябрами (тот же дефект, что был у отчёта owner_check.sh).
    std::printf("[probe] cold-start scan: backend '%s' reports NO pad on any of %d slots.\n"
                "        If one IS plugged in, the OS is not handing it over - the engine polls\n"
                "        every slot every frame, so it will show up by itself the moment the OS\n"
                "        reports it. Windows: Game Bar and Steam Input both take pads for\n"
                "        themselves (quit Steam; Settings -> Gaming -> Xbox Game Bar -> Off).\n"
                "        Linux: /dev/input/event* permissions (group 'input'), and Steam again.\n",
                pad != nullptr ? pad->backend_name() : "none", ::input::MAX_DEVICES);
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
