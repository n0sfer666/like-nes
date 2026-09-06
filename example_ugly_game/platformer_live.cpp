#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "assets_path.hpp"
#include "platform_args.hpp"
#include "platformer_live_input.hpp"
#include "platformer_scene.hpp"
#include "platformer_window.hpp"

// Живая половина образца-платформера (гейт 8 спеки #16): часы и один прогон. Всё остальное — чужое
// и общее: шаг мира тот же `step_stage`, что гоняет sim-голден, окно и ввод те же, что у сетевой
// цели (гейт 9 спеки #22).
//
// Тонким он остаётся нарочно: у окна нет ни одного утверждения, которое можно проверить на раннере,
// поэтому каждая строка, которую МОЖНО унести в чистую половину, туда унесена. Окно и ввод уехали в
// свои файлы, когда целей с окном стало две, — по той же границе: копия настройки поверхности и
// раскладки разошлась бы молча, и владелец играл бы в две разные игры.
namespace platformer {
namespace {

int run(const std::string& bundle) {
    Stage stage;
    if (!load_stage(bundle, stage)) {
        std::fprintf(stderr, "[platformer] level unreadable: %s\n", bundle.c_str());
        return 1;
    }
    Window win;
    if (!win.open("like-nes - platformer")) return 1;
    LiveInput in;
    if (!in.open(win.handle(), bundle)) {
        win.close();
        return 1;
    }
    std::printf("[platformer] WASD/arrows = move | space/up = jump | down+jump = drop | "
                "gamepad: %s | Esc = quit\n", in.pad_name());

    // Часы СВОИ, а не вертикальная синхронизация: показ на `Fifo` идёт со скоростью монитора, и
    // привязанный к нему шаг мира на 120 Гц играл бы вдвое быстрее собственной физики.
    const auto period = std::chrono::nanoseconds(tick_period_ns());
    auto next = std::chrono::steady_clock::now();
    for (uint32_t t = 0;; ++t) {
        next += period;
        std::this_thread::sleep_until(next);
        win.poll();
        if (win.quit_asked()) break;
        step_stage(stage, in.read(t));
        win.draw(stage);
    }
    win.close();
    std::printf("[platformer] window clean exit\n");
    return 0;
}

} // namespace
} // namespace platformer

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string bundle = game::resolve_bundle_path();
    for (int i = 1; i < argc; ++i) bundle = argv[i];
    if (bundle.empty()) {
        std::fprintf(stderr, "[platformer] game.bundle not found next to the executable\n");
        return 1;
    }
    return platformer::run(bundle);
}
