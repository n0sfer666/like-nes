#include "rumble_roster_quads.hpp"

namespace rumble {

framework::Vec2 screen_plane(const framework::brawl::Body& body) {
    return {body.pos.x, body.pos.z - body.pos.y};
}

FighterStats draw_roster(FighterQuads& quads, const Fighters& fighters, const Brawl& brawl, Layers& layers,
                         LayerStats& st, uint32_t level_textures, bool overlay) {
    FighterStats sum;
    for (const uint32_t i : brawl.draw_order()) {
        const framework::brawl::Body& b = brawl.body(i);
        const FighterStats fs = quads.add(fighters[i], fighters[i].pose(b, Brawl::PROFILE), screen_plane(b), layers,
                                          st, sheet_texture(level_textures, i), solid_texture(level_textures),
                                          overlay);
        sum.overlay += fs.overlay;
        sum.rejected += fs.rejected;
        sum.dropped += fs.dropped;
    }
    return sum;
}

} // namespace rumble
