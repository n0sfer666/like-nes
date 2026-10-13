#include "rumble_screen.hpp"

#include <cstdio>

#include "rumble_roster.hpp"
#include "rumble_roster_quads.hpp"
#include "rumble_stage_quads.hpp"

namespace rumble {

using framework::graphics::ViewportFit;

void Screen::reset() {
    shown = 0;
    following_ = false;
}

LayerStats Screen::menu(const Menu& m, const Credits& credits, const ViewportFit& fit, uint64_t tick) {
    LayerStats st;
    st.scale = fit.scale;
    const MenuStats ms = menu_.add(m, credits, fit, layers_, st);
    if (ms.unknown + ms.dropped > 0 && !menu_warned_) {
        std::fprintf(stderr, "neon-rumble: tick %llu: menu %u unknown glyph(s), %u dropped\n",
                     static_cast<unsigned long long>(tick), ms.unknown, ms.dropped);
        menu_warned_ = true;
    }
    credits_page(credits, fit, st, MENU_FONT_TEXTURE, MENU_SOLID_TEXTURE, tick);
    return st;
}

LayerStats Screen::level(const Play& play, const Hotseat& hotseat, const Credits& credits, const ViewportFit& fit,
                         uint64_t tick) {
    if (follow_point(play.brawl, follow_)) following_ = true;
    LayerStats st = layers_.build(play.level, fit, tick, following_ ? &follow_ : nullptr);
    const uint32_t textures = play.level.texture_count;
    const FighterStats ss = draw_stage(fighters_, play.brawl, layers_, st, solid_texture(textures));
    FighterStats fs = draw_roster(fighters_, play.fighters, play.brawl, layers_, st, textures, overlay);
    fs += ss;
    banner_.add(credits, banner_text(hotseat.lobby()), fit, layers_, st, atlas_texture(textures),
                solid_texture(textures));
    if (fs.rejected + fs.dropped > 0 && !fighter_warned_) {
        std::fprintf(stderr, "neon-rumble: tick %llu: fighters %u quad(s) rejected, %u dropped\n",
                     static_cast<unsigned long long>(tick), fs.rejected, fs.dropped);
        fighter_warned_ = true;
    }
    credits_page(credits, fit, st, atlas_texture(textures), solid_texture(textures), tick);
    return st;
}

void Screen::credits_page(const Credits& credits, const ViewportFit& fit, LayerStats& st, uint32_t font_texture,
                          uint32_t solid_texture, uint64_t tick) {
    const CreditStats cs =
        shown > 0 ? credits_.add(credits, shown - 1, fit, layers_, st, font_texture, solid_texture) : CreditStats{};
    if (shown > cs.pages) shown = 0;
    if (cs.unknown + cs.dropped > 0 && !credits_warned_) {
        std::fprintf(stderr, "neon-rumble: tick %llu: credits %u unknown glyph(s), %u dropped\n",
                     static_cast<unsigned long long>(tick), cs.unknown, cs.dropped);
        credits_warned_ = true;
    }
}

} // namespace rumble
