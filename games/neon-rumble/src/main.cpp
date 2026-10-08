#include <charconv>
#include <cstdio>
#include <cstring>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "rumble.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_report.hpp"

namespace {

// atoi отдал бы 0 на «abc» молча, и оконный режим крутился бы без конца вместо ошибки usage.
bool parse_frames(const char* text, int& frames) {
    const char* end = text + std::strlen(text);
    const auto [ptr, ec] = std::from_chars(text, end, frames);
    return ec == std::errc() && ptr == end && frames > 0;
}

rumble::PlayerCommand scripted(uint32_t t) {
    rumble::PlayerCommand c;
    framework::brawl::BrawlInput& in = c.input;
    in.present = true;
    const bool right = t < 44 || (t >= 110 && t < 140) || (t >= 170 && t < 200) || t == 304 || t >= 306;
    in.move.move_x = fix32::from_int(right ? 1 : t >= 244 && t < 300 ? -1 : 0);
    in.move.move_z = fix32::from_int(t < 12 ? -1 : 0);
    if (t == 44 || t == 56 || t == 68) c.attack = rumble::Attack::Punch;
    if (t == 140) in.buttons = framework::brawl::button::JUMP;
    if (t == 141) c.attack = rumble::Attack::Kick;
    if (t == 142) c.attack = rumble::Attack::Cross;
    if (t == 143) c.attack = rumble::Attack::Punch;
    if (t == 200) c.attack = rumble::Attack::Cross;
    if (t == 229) c.attack = rumble::Attack::Punch;
    if (t == 312) c.attack = rumble::Attack::Kick;
    return c;
}

int run_headless(rumble::Scene& scene, const rumble::Fighters& fighters, int frames) {
    rumble::report_brawl(fighters, *scene.brawl, 0);
    for (int i = 0; i < frames; ++i) {
        scene.brawl->player = scripted(static_cast<uint32_t>(i));
        scene.step(static_cast<uint32_t>(i));
        rumble::report_step(*scene.brawl, scene.ticks);
    }
    rumble::report_brawl(fighters, *scene.brawl, scene.ticks);
    if (scene.ticks != static_cast<uint32_t>(frames)) {
        std::fprintf(stderr, "neon-rumble: %d frames ran %u ticks\n", frames, scene.ticks);
        return 1;
    }
    std::printf("neon-rumble: headless run ok, %u frames\n", scene.ticks);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    bool headless = false;
    int frames = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc
                   && parse_frames(argv[i + 1], frames)) {
            ++i;
        } else {
            std::fprintf(stderr, "usage: neon_rumble [--headless] [--frames N]\n");
            return 2;
        }
    }
    if (headless && frames <= 0) {
        std::fprintf(stderr, "neon-rumble: --headless needs --frames N with N > 0\n");
        return 2;
    }
    if (!rumble::report_library_bundle()) return 1;
    rumble::Level level;
    if (!level.open(platform::exe_dir() + "/game.bundle", "level1")) return 1;
    rumble::report_level(level);
    rumble::Fighters fighters;
    for (uint32_t i = 0; i < rumble::FIGHTERS; ++i)
        if (!fighters[i].open(level, rumble::ROSTER[i].fighter)) return 1;
    rumble::Brawl brawl;
    if (!brawl.open(level, fighters)) return 1;
    rumble::report_fighters(level, fighters, brawl);
    rumble::Credits credits;
    if (!credits.open(level)) return 1;
    rumble::report_credits(level, credits);
    rumble::Scene scene;
    if (!scene.init(brawl)) return 1;
    return headless ? run_headless(scene, fighters, frames) : rumble::run_window(scene, level, fighters, credits, frames);
}
