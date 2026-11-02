#include "rumble_mobile.hpp"

#include <cstdio>

#include "gpu.hpp"
#include "rumble_gpu_frame.hpp"
#include "rumble_roster.hpp"
#include "viewport_fit.hpp"

namespace rumble {

void MobileRumble::draw() {
    const SurfaceFrame sf = acquire_frame(spec_, *gpu_, "neon-rumble", warned_);
    lost_ = sf.quit;
    if (!sf.texture) return;
    const framework::graphics::ViewportFit fit = framework::graphics::viewport_fit(
        {spec_.width, spec_.height}, flow_.viewport().zone, flow_.viewport().limit);
    const Play* play = flow_.play();
    LayerStats st = play != nullptr ? screen_->level(*play, hotseat_, credits_, fit, ticks_)
                                   : screen_->menu(flow_.menu(), credits_, fit, ticks_);
    const uint32_t scene_runs = st.runs;
    if (play != nullptr && !demo_ && touch_.visible(hotseat_.touch_player() >= 0)) {
        const uint32_t textures = play->level.level.texture_count;
        touch_quads_.add(touch_.source(), credits_.font, touch_scale_, screen_->layers(), st,
                         solid_texture(textures), atlas_texture(textures));
    }
    if (!quads_.upload(screen_->layers().quads(st), spec_.width, spec_.height) && !upload_warned_) {
        std::fprintf(stderr, "neon-rumble: %u quad(s) not uploaded, frame left empty\n", st.quads);
        upload_warned_ = true;
    }
    WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
    const auto runs = screen_->layers().runs(st);
    // Кнопки живут и в полосах за игровым viewport: их scissor — весь экран, а у сцены прежний shown.
    draw_frame(*gpu_, surface_, view, clear_, fit.shown, quads_, runs.first(scene_runs),
               QuadOverlay{{0, 0, spec_.width, spec_.height}, runs.subspan(scene_runs)});
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(sf.texture);
}

} // namespace rumble
