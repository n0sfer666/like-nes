#include "rumble_report.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "hash.hpp"
#include "platform_fs.hpp"
#include "rumble_layers.hpp"
#include "rumble_roster.hpp"
#include "source.hpp"

namespace rumble {

namespace {

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

} // namespace

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

// Строка доказывает, что нативный ввод этой ОС слинкован из префикса: без вызова линкер выбросил бы
// архив, и гейт sdk-game судил бы игру без него.
void report_pad_backend() {
    std::printf("neon-rumble: pad backend %s\n", ::input::make_gamepad_source()->backend_name());
}

// Сводка уровня и кадра — то, что headless доказывает о бандле без окна: таблица `visual` читается,
// каждая текстура карты лежит сырым RGBA8, политика вьюпорта на 960x540 и на окне меньше зоны,
// камера в границах `bounds`, кадр 960x540 весь ложится в квады.
void report_level(const Level& level) {
    const auto& row = *level.map.row;
    std::printf("neon-rumble: level %s %ux%u tile %u, %zu visual layer(s), %zu animated tile(s), %u texture(s)",
                level.name, row.width, row.height, row.tile_size, level.map.layers.size(),
                level.map.anims.size(), level.texture_count);
    for (uint32_t i = 0; i < level.texture_count; ++i)
        std::printf(" %ux%u", level.sizes[i].w, level.sizes[i].h);
    std::printf("\n");
    const ViewportFit fit = report_viewport(960, 540);
    report_viewport(300, 200);
    Layers layers;
    const LayerStats st = layers.build(level, fit, 0);
    const auto& b = level.bounds;
    const auto& c = layers.frame().camera.center;
    std::printf("neon-rumble: bounds %d..%d x %d..%d, camera %d,%d\n", b.min_x.to_int(), b.max_x.to_int(),
                b.min_y.to_int(), b.max_y.to_int(), c.x.to_int(), c.y.to_int());
    std::printf("neon-rumble: frame 960x540 scale %u: %u sprite(s), %u run(s), %u unknown, "
                "%u rejected, %u dropped\n",
                st.scale, st.sprites, st.runs, st.unknown, st.rejected, st.dropped);
}

// Сводка титров: шрифт и атлас из бандла, строки секции `credits` и раскладка всех страниц экрана F1 на
// 960x540 — ноль неизвестных символов доказывает, что кириллица заголовка и тире есть в шрифте.
void report_credits(const Level& level, const Credits& credits) {
    const auto& f = *credits.font.row;
    std::printf("neon-rumble: font %s line %u, %zu glyph(s), atlas %ux%u\n", Credits::FONT, f.line_height,
                credits.font.glyphs.size(), credits.atlas.width, credits.atlas.height);
    for (uint32_t i = 0; i < credits.table.count(); ++i) {
        const framework::core::Credit c = credits.table.at(i);
        std::printf("neon-rumble: credit %s | %s | %s | %s\n", c.pack, c.author, c.license, c.url);
    }
    Layers layers;
    CreditsQuads quads;
    const ViewportFit fit = framework::graphics::viewport_fit({960, 540});
    CreditStats sum;
    uint32_t drawn = 0;
    uint32_t pages = 1;
    for (uint32_t page = 0; page < pages; ++page) {
        LayerStats st = layers.build(level, fit, 0);
        const uint32_t before = st.quads;
        const uint32_t lost = page == 0 ? 0 : st.dropped;
        const CreditStats cs =
            quads.add(credits, page, fit, layers, st, atlas_texture(level.texture_count), solid_texture(level.texture_count));
        pages = cs.pages;
        sum.lines += cs.lines;
        sum.glyphs += cs.glyphs;
        sum.unknown += cs.unknown;
        sum.dropped += cs.dropped + st.dropped - lost;
        drawn += st.quads - before;
    }
    std::printf("neon-rumble: credits screen 960x540 scale %u: %u pack(s), %u page(s), %u line(s), %u glyph(s), "
                "%u unknown, %u quad(s), %u dropped\n",
                fit.scale, credits.table.count(), pages, sum.lines, sum.glyphs, sum.unknown, drawn, sum.dropped);
}

} // namespace rumble
