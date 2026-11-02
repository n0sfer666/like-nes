#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

#include "gpu.hpp"
#include "mobile_app.hpp"
#include "mobile_host.hpp"
#include "platform_args.hpp"

namespace {
std::string assets;
bool demo;
int fails;

void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("mobile-cpu: FAIL %s\n", why); } }
void frames(mobile::Game& game, int count) { for (int i = 0; i < count; ++i) game.frame(1.0 / 60); }
bool owns(const mobile::Game& game, const char* field) {
    const std::string state = " " + game.touch_state() + " ";
    return state.find(" " + std::string(field) + " ") != std::string::npos;
}
} // namespace

// Мок лишь хоста: GPU не инициализируется, поверхность не подключается, игровой CPU-цикл настоящий.
std::string mobile::asset_path(const char* name) { return assets + "/" + name; }
bool mobile::has_arg(const char* flag) { return demo && std::strcmp(flag, "--demo") == 0; }
void mobile::interactive_thread() {}
std::string mobile::device_info() { return "CPU test host"; }
bool mobile_cpu_probe(mobile::Game& game);

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc != 3) { std::puts("usage: rumble_mobile_cpu <assets-dir> <demo|idle|move|cancel|rotate|suspend|late|probe>"); return 1; }
    assets = argv[1];
    const std::string mode = argv[2];
    demo = mode == "demo";
    if (!demo && mode != "idle" && mode != "move" && mode != "cancel" && mode != "rotate" &&
        mode != "suspend" && mode != "late" && mode != "probe") return 1;
    GpuContext gpu;
    auto game = mobile::make_game();
    if (!game->init(gpu)) { std::puts("mobile-cpu: FAIL init"); return 1; }
    const mobile::SafeArea safe{44, 0, 44, 21, 3};
    game->resize(2532, 1170, safe);
    if (mode == "probe") {
        check(mobile_cpu_probe(*game), "valid probe input stream");
    } else if (demo) {
        frames(*game, 480);
    } else {
        if (mode == "late") frames(*game, 60);
        game->pointer(1, mobile::Touch::Down, 732, 301);
        check(owns(*game, "a=1"), "actual MobileRumble accepts a button in points after fb/scale");
        frames(*game, 1);
        game->pointer(1, mobile::Touch::Up, 732, 301);
        frames(*game, 1);
        if (mode != "idle" && mode != "late") {
            game->pointer(2, mobile::Touch::Down, 200, 240);
            check(owns(*game, "knob_x=200.000"), "probe exposes the actual source knob at down");
            game->pointer(2, mobile::Touch::Move, 248, 240);
            check(owns(*game, "stick=2"), "actual MobileRumble receives the floating stick");
            check(owns(*game, "knob_x=248.000"), "probe exposes the routed source move");
        }
        frames(*game, 58);
        if (mode == "cancel") game->cancel();
        if (mode == "suspend") { game->suspend(); game->resume(); }
        if (mode == "rotate") {
            game->resize(2532, 1170, {68, 0, 20, 21, 3});
            check(owns(*game, "stick=-1"), "safe-only rotation cancels the active stick");
            // У нового круга центр756, у старого732:776 внутри нового и вне старого радиуса30.
            game->pointer(3, mobile::Touch::Down, 776, 301);
            check(owns(*game, "a=3"), "button follows a changed cutout without a framebuffer resize");
            game->pointer(3, mobile::Touch::Move, std::numeric_limits<float>::quiet_NaN(), 301);
            check(owns(*game, "a=-1"), "nonfinite platform coordinates cancel before fix32 conversion");
        }
        if (mode == "cancel" || mode == "rotate" || mode == "suspend")
            check(owns(*game, "stick=-1"), "cancel releases ownership");
        frames(*game, 60);
    }
    game->detach_surface();
    game->shutdown();
    std::puts(fails ? "mobile-cpu: FAIL" : "mobile-cpu: PASS");
    return fails ? 1 : 0;
}
