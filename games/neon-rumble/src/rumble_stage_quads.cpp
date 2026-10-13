#include "rumble_stage_quads.hpp"

#include "rope_swing.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;
namespace sc = framework::scene;
using framework::graphics::Rect16;

constexpr uint32_t BELT_RGBA = 0x30c0c0ffu;
constexpr uint32_t LIFT_RGBA = 0xe0e040ffu;
constexpr uint32_t ROPE_RGBA = 0xc0a080ffu;
constexpr int32_t ROPE_DOTS = 8;
constexpr int16_t DOT = 2;
constexpr int16_t ROPE_END = 6;

Rect16 sized(fix32 w, fix32 h) { return {0, 0, static_cast<int16_t>(w.to_int()), static_cast<int16_t>(h.to_int())}; }

Rect16 centered(int16_t side) {
    const int16_t half = static_cast<int16_t>(side / 2);
    return {static_cast<int16_t>(-half), static_cast<int16_t>(-half), side, side};
}

} // namespace

FighterStats draw_stage(FighterQuads& quads, const Brawl& brawl, Layers& layers, LayerStats& st,
                        uint32_t solid_texture) {
    FighterStats out;
    const Platform& p = brawl.wave.platform;
    if (!brawl.wave.on || !p.movers || !p.ropes) return out;
    const sc::LevelObjects& level = brawl.wave.level;
    for (const sc::Conveyor& c : level.conveyors) {
        const sc::LevelRect& r = c.area.rect;
        out += quads.add_box(sized(r.x1 - r.x0, r.y1 - r.y0), false, {r.x0, r.y0}, BELT_RGBA, layers, st,
                             solid_texture);
    }
    for (uint32_t i = 0; i < level.paths.size(); ++i) {
        const sc::Path& path = level.paths[i];
        const framework::Vec2 at = p.movers->world.bodies()[i].position;
        const Rect16 box{static_cast<int16_t>(-path.w / 2), static_cast<int16_t>(-path.h / 2),
                         static_cast<int16_t>(path.w), static_cast<int16_t>(path.h)};
        out += quads.add_box(box, false, at, LIFT_RGBA, layers, st, solid_texture);
    }
    for (uint32_t i = 0; i < p.ropes->count; ++i) {
        const sc::Rope& rope = level.ropes[i];
        const framework::Vec2 pivot = rope.mark.at;
        const framework::Vec2 end = br::rope_end(pivot, rope.length, p.ropes->at[i].arc);
        for (int32_t k = 1; k < ROPE_DOTS; ++k) {
            const framework::Vec2 dot = pivot + (end - pivot) * (fix32::from_int(k) / fix32::from_int(ROPE_DOTS));
            out += quads.add_box(centered(DOT), false, dot, ROPE_RGBA, layers, st, solid_texture);
        }
        out += quads.add_box(centered(ROPE_END), false, end, ROPE_RGBA, layers, st, solid_texture);
    }
    return out;
}

} // namespace rumble
