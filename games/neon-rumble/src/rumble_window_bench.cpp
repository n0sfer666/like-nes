#include "rumble_window_bench.hpp"

#include "platform_fs.hpp"
#include "rumble_credits.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_play.hpp"
#include "rumble_roster.hpp"

namespace rumble {

bool WindowBenchmark::open(asset::BundleView bundle, Hotseat& hotseat, std::string& why) {
    return bind_touch(bundle, hotseat, why) &&
        frames.open(platform::exe_dir() + "/frame-stats.bundle", FrameStatsSource::Desktop, why);
}

bool WindowBenchmark::bind_touch(asset::BundleView bundle, Hotseat& hotseat, std::string& why) {
    if (!touch_.load(bundle, why)) return false;
    touch_.source().bind(&hotseat.engine());
    return true;
}

void WindowBenchmark::feed(uint32_t width, uint32_t height, uint32_t tick) {
    using framework::touch::Control;
    using framework::touch::Phase;
    const auto w = fix32::from_int(static_cast<int>(width));
    const auto h = fix32::from_int(static_cast<int>(height));
    auto& touch = touch_.source();
    touch.set_view(w, h, {});
    const uint32_t phase = tick % 180;
    const auto punch = touch.button(static_cast<uint32_t>(Control::A));
    // First punch joins, later punches take the same action path as a tap on a phone.
    if (punch && (phase == 0 || phase == 42))
        touch.pointer(1, Phase::Down, punch->center.x, punch->center.y);
    if (phase == 1 || phase == 43) touch.pointer(1, Phase::Up, {}, {});
    if (phase == 2) touch.pointer(2, Phase::Down, w / fix32::from_int(4), h / fix32::from_int(2));
    if (phase == 3) {
        if (const auto stick = touch.stick_base()) {
            const auto dx = tick / 180 % 2 == 0 ? stick->radius : -stick->radius;
            touch.pointer(2, Phase::Move, stick->center.x + dx, stick->center.y);
        }
    }
    if (phase == 30) touch.pointer(2, Phase::Up, {}, {});
}

void WindowBenchmark::overlay(const Play& play, const Credits& credits, Layers& layers, LayerStats& st) {
    const uint32_t textures = play.level.level.texture_count;
    quads_.add(touch_.source(), credits.font, 1, layers, st, solid_texture(textures), atlas_texture(textures));
}

} // namespace rumble
