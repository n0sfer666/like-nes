#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_io.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_coop_script.hpp"
#include "rumble_demo.hpp"
#include "rumble_flow.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_join_report.hpp"
#include "rumble_net_headless.hpp"
#include "rumble_pad_report.hpp"
#include "rumble_report.hpp"
#include "rumble_wave_script.hpp"
#include "rumble_window.hpp"
#include "rumble_window_bench.hpp"

namespace {

// atoi отдал бы 0 на «abc» молча, и оконный режим крутился бы без конца вместо ошибки usage.
bool parse_frames(const char* text, int& frames) {
    const char* end = text + std::strlen(text);
    const auto [ptr, ec] = std::from_chars(text, end, frames);
    return ec == std::errc() && ptr == end && frames > 0;
}

// Прогон --wave играет один P1: ёмкость жетонов 1, и ИИ доводит его до hp 0 одним жетоном. С --coop
// оба места играют скрипт волны, и жетонов два.
int run_headless(rumble::Flow& flow, rumble::Play& play, rumble::Hotseat& hotseat, int frames, bool waves,
                 bool coop) {
    rumble::WaveScript wave;
    rumble::CoopScript pair;
    rumble::Brawl& brawl = play.brawl;
    const uint32_t& ticks = play.scene.ticks;
    rumble::report_brawl(brawl, 0);
    for (int i = 0; i < frames; ++i) {
        const auto t = static_cast<uint32_t>(i);
        if (!waves) rumble::demo_p2(hotseat.engine(), ticks + 1);
        if (coop) pair.keys(hotseat.engine(), ticks + 1);
        if (!hotseat.tick(ticks + 1, brawl.players)) {
            std::fprintf(stderr, "neon-rumble: tick %u paused without a pad\n", ticks + 1);
            return 1;
        }
        if (coop) pair.next(brawl, t);
        else brawl.players[rumble::PLAYER] = waves ? wave.next(brawl, t) : rumble::demo_p1(t);
        const framework::brawl::Seats was = brawl.seats;
        flow.run(t);
        rumble::report_step(brawl, ticks);
        rumble::report_joins(brawl, was, ticks);
    }
    rumble::report_brawl(brawl, ticks);
    if (ticks != static_cast<uint32_t>(frames)) {
        std::fprintf(stderr, "neon-rumble: %d frames ran %u ticks\n", frames, ticks);
        return 1;
    }
    std::printf("neon-rumble: headless run ok, %u frames\n", ticks);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc > 1 && rumble::network_command(argv[1])) {
        return rumble::run_network_headless(argc, argv);
    }
    bool headless = false;
    bool waves = false;
    bool coop = false;
    bool frame_stats = false;
    const char* level_name = "level1";
    int frames = 0;
    const char* script_path = nullptr;
    const char* shots = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!std::strcmp(argv[i], "--wave")) {
            waves = true;
        } else if (!std::strcmp(argv[i], "--coop")) {
            coop = true;
        } else if (!std::strcmp(argv[i], "--frame-stats")) {
            frame_stats = true;
        } else if (!std::strcmp(argv[i], "--level") && i + 1 < argc) {
            level_name = argv[++i];
        } else if (!std::strcmp(argv[i], "--script") && i + 1 < argc) {
            script_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--shots") && i + 1 < argc) {
            shots = argv[++i];
        } else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc
                   && parse_frames(argv[i + 1], frames)) {
            ++i;
        } else {
            std::fprintf(stderr, "usage: neon_rumble [--headless] [--wave [--coop]] [--level NAME] [--frames N] "
                                 "[--script FILE [--shots DIR]] | --frame-stats\n");
            return 2;
        }
    }
    if (frame_stats && argc != 2) {
        std::fprintf(stderr, "neon-rumble: --frame-stats is a standalone window run\n");
        return 2;
    }
    if (coop && (!waves || !headless)) {
        std::fprintf(stderr, "neon-rumble: --coop is a headless --wave run\n");
        return 2;
    }
    if (shots != nullptr && script_path == nullptr) {
        std::fprintf(stderr, "neon-rumble: --shots names where a --script run writes its frames\n");
        return 2;
    }
    if (script_path != nullptr && headless) {
        std::fprintf(stderr, "neon-rumble: --script drives the window, not a headless run\n");
        return 2;
    }
    rumble::WindowScript script;
    std::string why;
    if (script_path != nullptr && !script.open(script_path, why)) {
        std::fprintf(stderr, "neon-rumble: %s\n", why.c_str());
        return 2;
    }
    if (headless && frames <= 0) {
        std::fprintf(stderr, "neon-rumble: --headless needs --frames N with N > 0\n");
        return 2;
    }
    if (!rumble::report_library_bundle(platform::exe_dir() + "/library.bundle")) return 1;
    rumble::report_pad_backend();
    const std::string path = platform::exe_dir() + "/game.bundle";
    platform::MappedFile file;
    asset::BundleView bundle;
    if (!file.open(path) || !bundle.open(file.data(), file.size(), false)) {
        std::fprintf(stderr, "neon-rumble: cannot open %s\n", path.c_str());
        return 1;
    }
    rumble::Credits credits;
    if (!credits.open(bundle)) return 1;
    rumble::Menu menu;
    rumble::Flow flow(bundle, credits, menu);
    if (!menu.init(level_name, waves) || !flow.open()) return 1;
    rumble::Hotseat hotseat;
    if (!headless && !hotseat.open(frame_stats)) return 1;
    std::unique_ptr<rumble::WindowBenchmark> benchmark;
    if (frame_stats) {
        benchmark = std::make_unique<rumble::WindowBenchmark>();
        if (!benchmark->open(bundle, hotseat, why)) {
            std::fprintf(stderr, "neon-rumble: %s\n", why.c_str());
            return 1;
        }
    }
    const rumble::WindowRun run{frames, script_path != nullptr ? &script : nullptr,
        shots != nullptr ? shots : ".", benchmark.get()};
    if (!headless) {
        const int result = rumble::run_window(flow, hotseat, credits, run);
        const bool complete = !benchmark || benchmark->frames.finish(true);
        return complete ? result : 1;
    }
    // Headless обходит меню: пункт `--level` входит сразу, отказ — строка `menu:` и код 0.
    flow.enter();
    flow.apply();
    rumble::Play* play = flow.play();
    if (play == nullptr) return 0;
    if (!hotseat.open()) return 1;
    return run_headless(flow, *play, hotseat, frames, waves, coop);
}
