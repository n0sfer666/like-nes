#include "rumble_layers.hpp"

#include "layer_draw.hpp"
#include "layer_quads.hpp"
#include "viewport.hpp"

namespace rumble {

namespace {

using framework::graphics::LayerFrame;

// Левый край вида ходит 0 → span → 0 по пикселю за тик. Ширина вида — та же дробная `2·half`, что
// у камеры: целое `screen / zoom` при нецелом частном уводило правый край за карту, в цвет очистки.
// Карта уже вида стоит по центру.
fix32 scroll_x(uint32_t map_w, fix32 half, uint64_t tick) {
    const fix32 room = fix32::from_int(static_cast<int32_t>(map_w)) - half - half;
    if (room.raw <= 0) return fix32::from_int(static_cast<int32_t>(map_w / 2));
    const uint64_t span = static_cast<uint64_t>(room.raw / fix32::ONE);
    if (span == 0) return half;
    const uint64_t pos = tick % (2 * span);
    const uint64_t left = pos <= span ? pos : 2 * span - pos;
    return fix32::from_int(static_cast<int32_t>(left)) + half;
}

fix32 settle_y(uint32_t map_h, fix32 half) {
    const fix32 bottom = fix32::from_int(static_cast<int32_t>(map_h));
    if (bottom.raw <= (half + half).raw) return fix32::from_int(static_cast<int32_t>(map_h / 2));
    return bottom - half;
}

} // namespace

Layers::Layers()
    : sprites_(CAPACITY), keys_(CAPACITY), batches_(CAPACITY), quads_(CAPACITY), runs_(CAPACITY) {}

LayerStats Layers::build(const Level& level, uint32_t screen_w, uint32_t screen_h, uint64_t tick) {
    const auto& row = *level.map.row;
    const uint32_t map_w = row.width * row.tile_size;
    const uint32_t map_h = row.height * row.tile_size;
    LayerStats st;
    st.zoom = map_h > 0 && screen_h / map_h > 1 ? screen_h / map_h : 1;

    LayerFrame f;
    f.view.screen_half = {fix32::from_int(static_cast<int32_t>(screen_w)) / fix32::from_int(2),
                          fix32::from_int(static_cast<int32_t>(screen_h)) / fix32::from_int(2)};
    f.view.zoom = fix32::from_int(static_cast<int32_t>(st.zoom));
    const framework::Vec2 half = framework::graphics::viewport_half_world(f.view);
    f.camera.center = {scroll_x(map_w, half.x, tick), settle_y(map_h, half.y)};
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
    return st;
}

} // namespace rumble
