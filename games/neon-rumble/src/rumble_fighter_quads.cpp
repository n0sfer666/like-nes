#include "rumble_fighter_quads.hpp"

#include "camera.hpp"
#include "cel_quads.hpp"
#include "clip_debug.hpp"
#include "viewport.hpp"

namespace rumble {

namespace {

using namespace framework::graphics;

DebugGlyphs solid_glyphs() {
    DebugGlyphs g;
    g.has_solid = true;
    return g;
}

CelPlace place(framework::Vec2 world, bool flip, const Layers& layers, const LayerStats& st) {
    const LayerFrame& f = layers.frame();
    const framework::Vec2 center = camera_layer_center(f.camera, f.config, f.tick, fix32::from_int(1));
    const framework::Vec2 at = world_to_screen_snapped(f.view, center, world);
    return {at.x.to_int(), at.y.to_int(), static_cast<int32_t>(st.scale), flip};
}

fix32 whole(int32_t v) { return fix32::from_int(v); }

} // namespace

FighterQuads::FighterQuads() : debug_(OVERLAY), quads_(OVERLAY) {}

FighterStats FighterQuads::add(const Fighter& fighter, const Pose& pose, framework::Vec2 world, Layers& layers,
                               LayerStats& st, uint32_t sheet_texture) {
    FighterStats out;
    const CelPlace at = place(world, pose.flip, layers, st);
    const ClipCel* cel = frame_cel(pose.clip, pose.frame);
    if (cel != nullptr && cel_quad(*cel, at, fighter.sheet_size, quads_[0]))
        layers.append(st, {quads_.data(), 1}, sheet_texture);
    else
        ++out.rejected;
    return out;
}

FighterStats FighterQuads::add_overlay(const Pose& pose, framework::Vec2 world, const DepthOverlay& depth,
                                       Layers& layers, LayerStats& st, uint32_t solid_texture) {
    FighterStats out;
    const CelPlace at = place(world, pose.flip, layers, st);
    DebugDraw dd(debug_.data(), OVERLAY, solid_glyphs());
    draw_cel_debug(dd, pose.clip, pose.frame, at);
    draw_depth(dd, pose, at, depth);
    const DebugQuadStats d = debug_quads({debug_.data(), dd.count()}, quads_);
    layers.append(st, {quads_.data(), d.quads}, solid_texture);
    out.overlay = d.quads;
    out.rejected = d.rejected;
    out.dropped = dd.dropped() + d.dropped;
    return out;
}

FighterStats FighterQuads::add_box(const Rect16& box, bool flip, framework::Vec2 world, uint32_t rgba, Layers& layers,
                                  LayerStats& st, uint32_t solid_texture) {
    FighterStats out;
    const ScreenRect r = box_screen_rect(box, place(world, flip, layers, st));
    const framework::Vec2 half{whole(r.w) / whole(2), whole(r.h) / whole(2)};
    DebugDraw dd(debug_.data(), OVERLAY, solid_glyphs());
    dd.fill({whole(r.x) + half.x, whole(r.y) + half.y}, half, rgba);
    const DebugQuadStats d = debug_quads({debug_.data(), dd.count()}, quads_);
    layers.append(st, {quads_.data(), d.quads}, solid_texture);
    out.rejected = d.rejected;
    out.dropped = dd.dropped() + d.dropped;
    return out;
}

} // namespace rumble
