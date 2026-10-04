#include "rumble_layers.hpp"

#include <algorithm>

#include "layer_draw.hpp"
#include "layer_quads.hpp"

namespace rumble {

namespace {

using framework::graphics::CameraBounds;
using framework::graphics::LayerFrame;

framework::Vec2 sweep(const CameraBounds& b, framework::Vec2 half, uint64_t tick) {
    const fix32 lo = b.min_x + half.x;
    const fix32 room = b.max_x - half.x - lo;
    const uint64_t span = room.raw > 0 ? static_cast<uint64_t>(room.raw / fix32::ONE) : 0;
    if (span == 0) return {lo, b.max_y};
    const uint64_t pos = tick % (2 * span);
    const uint64_t left = pos <= span ? pos : 2 * span - pos;
    return {lo + fix32::from_int(static_cast<int32_t>(left)), b.max_y};
}

} // namespace

Layers::Layers()
    : sprites_(CAPACITY), keys_(CAPACITY), batches_(CAPACITY), quads_(CAPACITY), runs_(CAPACITY) {}

LayerStats Layers::build(const Level& level, const framework::graphics::ViewportFit& fit, uint64_t tick) {
    LayerStats st;
    st.scale = fit.scale;

    LayerFrame f;
    f.view = fit.view;
    f.config.policies = framework::graphics::CAMERA_BOUNDS;
    f.config.half_view = framework::graphics::view_zone_half();
    f.config.bounds = level.bounds;
    framework::graphics::camera_follow(f.camera, f.config, sweep(level.bounds, f.config.half_view, tick), 0);
    f.tick = tick;
    f.textures = {level.guids, level.texture_count};

    framework::graphics::SpriteList list(sprites_.data(), keys_.data(), CAPACITY);
    for (uint32_t i = 0; i < level.map.layers.size(); ++i)
        st.unknown += framework::graphics::draw_layer(list, level.map, level.map.layers[i], f,
                                                      static_cast<int16_t>(i)).unknown;
    const uint32_t batches = list.build(batches_.data(), CAPACITY);
    const framework::graphics::QuadStats q = framework::graphics::layer_quads(
        list, {batches_.data(), batches}, level.map, {level.sizes, level.texture_count}, quads_,
        runs_);
    st.sprites = list.count();
    st.quads = q.quads;
    st.runs = q.runs;
    st.rejected = q.rejected;
    st.dropped = list.dropped() + q.dropped;
    frame_ = f;
    return st;
}

void Layers::append(LayerStats& st, std::span<const render::Quad> extra, uint32_t texture) {
    if (extra.empty()) return;
    if (st.runs == CAPACITY || extra.size() > CAPACITY - st.quads) {
        st.dropped += static_cast<uint32_t>(extra.size());
        return;
    }
    std::copy(extra.begin(), extra.end(), quads_.begin() + st.quads);
    runs_[st.runs++] = {st.quads, static_cast<uint32_t>(extra.size()), texture};
    st.quads += static_cast<uint32_t>(extra.size());
}

} // namespace rumble
