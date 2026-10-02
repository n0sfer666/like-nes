#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hash.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "rumble.hpp"
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

// Сводка уровня и кадра — то, что headless доказывает о бандле без окна: таблица `visual` читается,
// каждая текстура карты лежит сырым RGBA8, кадр в окне 960x540 весь ложится в квады.
void report_level(const rumble::Level& level) {
    const auto& row = *level.map.row;
    std::printf("neon-rumble: level %s %ux%u tile %u, %zu visual layer(s), %u texture(s)",
                level.name, row.width, row.height, row.tile_size, level.map.layers.size(),
                level.texture_count);
    for (uint32_t i = 0; i < level.texture_count; ++i)
        std::printf(" %ux%u", level.sizes[i].w, level.sizes[i].h);
    std::printf("\n");
    rumble::Layers layers;
    const rumble::LayerStats st = layers.build(level, 960, 540, 0);
    std::printf("neon-rumble: frame 960x540 zoom %u: %u sprite(s), %u run(s), %u unknown, "
                "%u rejected, %u dropped\n",
                st.zoom, st.sprites, st.runs, st.unknown, st.rejected, st.dropped);
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
    rumble::Scene scene;
    if (!scene.init()) return 1;
    return headless ? run_headless(scene, frames) : rumble::run_window(scene, level, frames);
}
