#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "engine.hpp"
#include "pad_registry.hpp"
#include "platform_args.hpp"
#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "preset_bake.hpp"
#include "presets.hpp"
#include "probe_axis_report.hpp"
#include "probe_report.hpp"
#include "rebind_session.hpp"
#include "rebind_store.hpp"
#include "source.hpp"
#include "source_names.hpp"

// Гейт 8 спеки #14 (owner-HW): то, что нельзя проверить на раннере — живой пад, его паспорт,
// выбранный по паспорту профиль, перебинд в рантайме с конфликтом и отключение пада на ходу.
// Пресет берётся из ТЕКСТОВОГО манифеста (аргумент; по умолчанию — встроенный), поэтому probe
// живёт в движке и ничего не знает про игру-образец: её манифест ему просто передают путём.
//
// Запуск: framework_input_probe [manifest.txt]. Окно GLFW ловит клавиатуру и мышь, native-бэкенд
// (XInput / evdev / GameController) — пад. Накладка перебиндов пишется в каталог сейвов, поэтому
// «перебинд → перезапуск → раскладка сохранилась» проверяется вторым запуском probe'а.
namespace {

using namespace framework::input;

const char* FALLBACK_MANIFEST = R"(
preset | probe
action | fire | key:space | pad:south
action | jump | key:z | pad:east
axis   | move_x | key:d | key:a
axis   | move_x | padaxis:lx | -
axis   | move_y | key:w | key:s
axis   | move_y | padaxis:-ly | -
shape  | move_x | 0.18 | 1.0 | 1 | move_y
shape  | move_y | 0.18 | 1.0 | 1 | move_x

pad | Microsoft Xbox      | 0x045e | -      | Xbox      | xbox        | 0.18 | 0.12
pad | Sony DualSense      | 0x054c | 0x0ce6 | DualSense | playstation | 0.10 | 0.10
pad | Nintendo Switch Pro | 0x057e | -      | Nintendo  | nintendo    | 0.15 | 0.12
)";

const char* PROFILE_FILE = "controls_probe.txt";

std::string save_path() {
    std::string dir;
    if (!platform::env_var("LIKENES_SAVE_DIR", dir) || dir.empty())
        dir = platform::user_data_dir("like-nes");
    if (dir.empty() || !platform::ensure_dir(dir)) dir = platform::exe_dir();
    return dir + "/" + PROFILE_FILE;
}

void rebuild(const PresetTable& table, uint32_t preset, const RebindStore& store,
             ::input::ActionMap& map) {
    map = ::input::ActionMap{};
    table.bind(preset, map);
    store.apply(table, preset, map);
    ::input::PlayerAssign pa;
    pa.use_kbd_mouse = true;
    pa.pad_slot = 0;
    map.assign_player(0, pa);
}

bool edge(GLFWwindow* win, int key, bool& prev) {
    const bool now = glfwGetKey(win, key) == GLFW_PRESS;
    const bool fired = now && !prev;
    prev = now;
    return fired;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::string manifest = FALLBACK_MANIFEST;
    if (argc > 1 && !platform::read_text(argv[1], manifest)) {
        std::fprintf(stderr, "[probe] cannot read manifest '%s'\n", argv[1]);
        return 1;
    }

    std::vector<uint8_t> blob;
    PresetBakeError err;
    if (!bake_presets(manifest, blob, err)) {
        std::fprintf(stderr, "[probe] manifest line %d: %s\n", err.line, err.message.c_str());
        return 1;
    }
    PresetTable table;
    if (!table.open(blob.data(), blob.size()) || table.preset_count() == 0) {
        std::fprintf(stderr, "[probe] preset table did not open\n");
        return 1;
    }
    // Перебор пределов движка (`MAX_ACTIONS`/`MAX_AXES`) спрашивать здесь нечем и незачем: проба
    // печёт манифест ПРЯМО НАД этой строкой, а `bake_presets` отбивает такой пресет с номером
    // строки. Проверка после бейка повторяла бы её хуже — без строки и без имени.
    const uint32_t preset = 0;
    // До окна и до бэкендов: пресет без осей движения нечем судить, а проба, доехавшая до отчёта,
    // напечатала бы правдоподобные числа не про ту ось.
    MoveAxes move;
    if (!resolve_move_axes(table, preset, move)) return 1;

    RebindStore store;
    std::string stored_preset;
    const std::string path = save_path();
    if (store.load(path, stored_preset) && stored_preset == table.preset_name(preset))
        std::printf("[probe] overlay loaded from %s: %zu edit(s) - the restart half of gate 4\n",
                    path.c_str(), store.items().size());
    else
        std::printf("[probe] no overlay for preset '%s' at %s - clean preset\n",
                    table.preset_name(preset), path.c_str());

    ::input::ActionMap map;
    rebuild(table, preset, store, map);
    ::input::InputEngine engine(map);

    if (!glfwInit()) {
        std::fprintf(stderr, "[probe] glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* win = glfwCreateWindow(640, 240, "like-nes input probe (Esc=quit)", nullptr, nullptr);
    if (win == nullptr) {
        glfwTerminate();
        return 1;
    }
    ::input::install_glfw_input(win, engine);
    ::input::GamepadSource* pad = ::input::make_gamepad_source();
    const bool have_pad = pad != nullptr && pad->init();
    std::printf("[probe] gamepad backend: %s\n", have_pad ? pad->backend_name() : "none");
    report_bindings(table, preset, store);

    PadRegistry reg;
    AxisWitness axes(move);
    RebindSession session;
    RebindConflict conflict;
    bool pad_prev[::input::MAX_DEVICES] = {};
    bool key_prev[16] = {};
    std::size_t log_seen = 0;
    uint32_t which = 0;

    const auto period = std::chrono::microseconds(16667);
    auto next = std::chrono::steady_clock::now();
    for (uint32_t t = 0; !glfwWindowShouldClose(win); ++t) {
        next += period;
        std::this_thread::sleep_until(next);
        glfwPollEvents();
        if (have_pad) pad->poll(engine);
        const ::input::InputFrame& f = engine.begin_tick(t, 0);
        report_pads(engine, pad, reg, table, pad_prev);
        // Первый тик уже содержит итог первого опроса — здесь он и называется вслух.
        if (t == 0) report_cold_start(engine, pad);
        // Свидетель осей смотрит КАЖДЫЙ тик, а не раз в тридцать, как строка состояния: отклонение
        // стика длится доли секунды, и выборка каждый тридцатый кадр его штатно пропускает.
        axes.observe(engine.device(), f);

        if (session.active() && !session.captured()) {
            const std::vector<::input::RawEvent>& log = engine.event_log();
            for (; log_seen < log.size() && !session.captured(); ++log_seen)
                if (session.feed(log[log_seen]))
                    std::printf("\n[probe] captured %s for '%s' slot %u - Enter applies (refuses "
                                "on conflict), F forces, C cancels\n",
                                source_name(session.candidate()).c_str(),
                                table.action_name(preset, static_cast<uint32_t>(session.action())),
                                session.which());
        }

        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
        if (session.active()) {
            const bool force = edge(win, GLFW_KEY_F, key_prev[0]);
            if (force || edge(win, GLFW_KEY_ENTER, key_prev[1])) {
                if (session.commit(table, preset, store, force, &conflict)) {
                    rebuild(table, preset, store, map);
                    engine.set_event_logging(false);
                    report_bindings(table, preset, store);
                } else if (conflict.action >= 0) {
                    std::printf("\n[probe] source already bound to '%s' slot %u - F takes it, C "
                                "cancels\n",
                                table.action_name(preset, static_cast<uint32_t>(conflict.action)),
                                conflict.which);
                }
            }
            if (edge(win, GLFW_KEY_C, key_prev[2])) {
                session.cancel();
                engine.set_event_logging(false);
                std::printf("\n[probe] rebind cancelled\n");
            }
            continue;
        }

        if (edge(win, GLFW_KEY_TAB, key_prev[3])) {
            which = which == 0 ? 1 : 0;
            std::printf("\n[probe] alternative slot for the next rebind: %u\n", which);
        }
        if (edge(win, GLFW_KEY_S, key_prev[4]))
            std::printf("\n[probe] overlay %s to %s\n",
                        store.save(path, table.preset_name(preset)) ? "saved" : "FAILED to save",
                        path.c_str());
        if (edge(win, GLFW_KEY_X, key_prev[5])) {
            store.reset_all();
            rebuild(table, preset, store, map);
            std::printf("\n[probe] overlay cleared (press S to persist the reset)\n");
            report_bindings(table, preset, store);
        }
        for (uint32_t a = 0; a < table.action_count(preset) && a < 9; ++a)
            if (edge(win, GLFW_KEY_1 + static_cast<int>(a), key_prev[6 + a])) {
                engine.set_event_logging(true);
                log_seen = engine.event_log().size();
                session.begin(static_cast<int>(a), which);
                std::printf("\n[probe] press any key/button for '%s' slot %u (C cancels)\n",
                            table.action_name(preset, a), which);
            }

        if ((t % 30) == 0) report_status(table, preset, f, reg, t, move);
    }

    axes.report();
    std::printf("\n[probe] bye - overlay %s\n",
                store.empty() ? "empty" : "in memory (S saves it, X clears it)");
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
