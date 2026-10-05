#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hash.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "rumble.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_layers.hpp"

namespace {

// library.bundle кладёт рядом с exe like_nes_bake: поиск от exe, а не от cwd, находит его при
// запуске из любого каталога — так же, как его найдёт игрок.
bool report_library_bundle() {
    const std::string path = platform::exe_dir() + "/library.bundle";
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes(path, bytes) || bytes.empty()) {
        std::fprintf(stderr, "neon-rumble: cannot read %s\n", path.c_str());
        return false;
    }
    std::printf("neon-rumble: library.bundle %zu bytes fnv1a %016llx\n", bytes.size(),
                static_cast<unsigned long long>(asset::fnv1a(bytes.data(), bytes.size())));
    return true;
}

// atoi отдал бы 0 на «abc» молча, и оконный режим крутился бы без конца вместо ошибки usage.
bool parse_frames(const char* text, int& frames) {
    const char* end = text + std::strlen(text);
    const auto [ptr, ec] = std::from_chars(text, end, frames);
    return ec == std::errc() && ptr == end && frames > 0;
}

using framework::graphics::PixelRect;
using framework::graphics::ViewportFit;

ViewportFit report_viewport(uint32_t w, uint32_t h) {
    const ViewportFit fit = framework::graphics::viewport_fit({w, h});
    const auto rect = [](const PixelRect& r) { std::printf("%d,%d %ux%u", r.x, r.y, r.w, r.h); };
    std::printf("neon-rumble: viewport %ux%u scale %u, visible %ux%u, zone ", w, h, fit.scale,
                fit.visible.w, fit.visible.h);
    rect(fit.zone);
    std::printf(", shown ");
    rect(fit.shown);
    std::printf(", %u strip(s)%s\n", fit.strip_count, fit.cropped ? ", cropped" : "");
    return fit;
}

// Сводка уровня и кадра — то, что headless доказывает о бандле без окна: таблица `visual` читается,
// каждая текстура карты лежит сырым RGBA8, политика вьюпорта на 960x540 и на окне меньше зоны,
// камера в границах `bounds`, кадр 960x540 весь ложится в квады.
void report_level(const rumble::Level& level) {
    const auto& row = *level.map.row;
    std::printf("neon-rumble: level %s %ux%u tile %u, %zu visual layer(s), %zu animated tile(s), %u texture(s)",
                level.name, row.width, row.height, row.tile_size, level.map.layers.size(),
                level.map.anims.size(), level.texture_count);
    for (uint32_t i = 0; i < level.texture_count; ++i)
        std::printf(" %ux%u", level.sizes[i].w, level.sizes[i].h);
    std::printf("\n");
    const ViewportFit fit = report_viewport(960, 540);
    report_viewport(300, 200);
    rumble::Layers layers;
    const rumble::LayerStats st = layers.build(level, fit, 0);
    const auto& b = level.bounds;
    const auto& c = layers.frame().camera.center;
    std::printf("neon-rumble: bounds %d..%d x %d..%d, camera %d,%d\n", b.min_x.to_int(), b.max_x.to_int(),
                b.min_y.to_int(), b.max_y.to_int(), c.x.to_int(), c.y.to_int());
    std::printf("neon-rumble: frame 960x540 scale %u: %u sprite(s), %u run(s), %u unknown, "
                "%u rejected, %u dropped\n",
                st.scale, st.sprites, st.runs, st.unknown, st.rejected, st.dropped);
}

// Сводка бойца: клипы и лист из бандла, спавн из таблицы объектов и поза на двух тиках витрины —
// второй берёт следующий круг, где боец развёрнут, и судит флип без окна.
void report_fighter(const rumble::Level& level, const rumble::Fighter& fighter) {
    std::printf("neon-rumble: fighter %u clip(s), sheet %ux%u, spawn %d,%d facing %s\n",
                fighter.clips.count(), fighter.sheet.width, fighter.sheet.height,
                fighter.spawn.x.to_int(), fighter.spawn.y.to_int(),
                fighter.faces_left ? "left" : "right");
    rumble::Layers layers;
    rumble::FighterQuads quads;
    const ViewportFit fit = framework::graphics::viewport_fit({960, 540});
    for (const uint64_t tick : {uint64_t{0}, uint64_t{215}}) {
        rumble::LayerStats st = layers.build(level, fit, tick);
        const rumble::Pose p = fighter.pose(tick);
        const rumble::FighterStats fs = quads.add(fighter, p, layers, st, level.texture_count, true);
        std::printf("neon-rumble: fighter tick %llu: %s frame %u flip %d, %zu hit, %zu hurt, "
                    "%zu push, %u overlay quad(s), %u rejected, %u dropped\n",
                    static_cast<unsigned long long>(tick), p.name, p.frame, p.flip ? 1 : 0,
                    framework::graphics::frame_boxes(p.clip, p.frame, framework::graphics::BoxKind::Hit).size(),
                    framework::graphics::frame_boxes(p.clip, p.frame, framework::graphics::BoxKind::Hurt).size(),
                    framework::graphics::frame_boxes(p.clip, p.frame, framework::graphics::BoxKind::Push).size(),
                    fs.overlay, fs.rejected, fs.dropped + st.dropped);
    }
}

int run_headless(rumble::Scene& scene, int frames) {
    for (int i = 0; i < frames; ++i) scene.step(static_cast<uint32_t>(i));
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
    if (!report_library_bundle()) return 1;
    rumble::Level level;
    if (!level.open(platform::exe_dir() + "/game.bundle", "level1")) return 1;
    report_level(level);
    rumble::Fighter fighter;
    if (!fighter.open(level)) return 1;
    report_fighter(level, fighter);
    rumble::Scene scene;
    if (!scene.init()) return 1;
    return headless ? run_headless(scene, frames) : rumble::run_window(scene, level, fighter, frames);
}
