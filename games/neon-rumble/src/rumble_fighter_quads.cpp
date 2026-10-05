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

CelPlace place(const Fighter& fighter, const Pose& pose, const Layers& layers, const LayerStats& st) {
    const LayerFrame& f = layers.frame();
    const framework::Vec2 center = camera_layer_center(f.camera, f.config, f.tick, fix32::from_int(1));
    const framework::Vec2 at = world_to_screen_snapped(f.view, center, fighter.spawn);
    return {at.x.to_int(), at.y.to_int(), static_cast<int32_t>(st.scale), pose.flip};
}

} // namespace

FighterQuads::FighterQuads() : debug_(OVERLAY), quads_(OVERLAY) {}

FighterStats FighterQuads::add(const Fighter& fighter, const Pose& pose, Layers& layers, LayerStats& st,
                               uint32_t sheet_texture, bool overlay) {
    FighterStats out;
    const CelPlace at = place(fighter, pose, layers, st);
    const ClipCel* cel = frame_cel(pose.clip, pose.frame);
    if (cel != nullptr && cel_quad(*cel, at, fighter.sheet_size, quads_[0]))
        layers.append(st, {quads_.data(), 1}, sheet_texture);
    else
        ++out.rejected;
    if (!overlay) return out;
    DebugDraw dd(debug_.data(), OVERLAY, solid_glyphs());
    draw_cel_debug(dd, pose.clip, pose.frame, at);
    const DebugQuadStats d = debug_quads({debug_.data(), dd.count()}, quads_);
    layers.append(st, {quads_.data(), d.quads}, sheet_texture + 1);
    out.overlay = d.quads;
    out.rejected += d.rejected;
    out.dropped = dd.dropped() + d.dropped;
    return out;
}

} // namespace rumble
