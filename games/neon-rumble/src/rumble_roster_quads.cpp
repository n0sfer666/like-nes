#include "rumble_roster_quads.hpp"

namespace rumble {

framework::Vec2 screen_plane(const framework::brawl::Body& body) {
    return {body.pos.x, body.pos.z - body.pos.y};
}

bool follow_point(const Brawl& brawl, framework::Vec2& out) {
    for (const framework::brawl::Seat& s : brawl.seats.at) {
        const framework::brawl::Body* b = s.present ? brawl.pool.find(s.body) : nullptr;
        if (b == nullptr) continue;
        out = screen_plane(*b);
        return true;
    }
    return false;
}

DepthOverlay depth_overlay(const Brawl& brawl, const framework::brawl::Body& b) {
    const framework::brawl::Archetype& a = brawl.kinds.types[b.kind];
    DepthOverlay d;
    d.lift = b.pos.y;
    d.body = a.depth;
    for (uint8_t k = 0; k < d.hit.size(); ++k) {
        uint8_t slot = 0;
        if (framework::brawl::find_move(a, b.move, k, slot)) d.hit[k] = a.moves[slot].strike.depth;
    }
    return d;
}

FighterStats draw_roster(FighterQuads& quads, const Fighters& fighters, const Brawl& brawl, Layers& layers,
                         LayerStats& st, uint32_t level_textures, bool overlay) {
    FighterStats sum;
    for (const uint32_t i : brawl.draw_order()) {
        const framework::brawl::Body& b = *brawl.find(i);
        const FighterStats fs = quads.add(fighters[i], fighters[i].pose(b), screen_plane(b), depth_overlay(brawl, b),
                                          layers, st, sheet_texture(level_textures, i), solid_texture(level_textures),
                                          overlay);
        sum.overlay += fs.overlay;
        sum.rejected += fs.rejected;
        sum.dropped += fs.dropped;
    }
    return sum;
}

} // namespace rumble
