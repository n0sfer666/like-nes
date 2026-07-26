#include "capture.hpp"
#include "gpu.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "sprite.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t W = 960, H = 540, FRAME = 90;

// T4-эталон НЕ побайтовый (GPU-рендер не бит-в-бит между вендорами/драйверами).
// Metal-эталон пиннут на эту машину; сравнение perceptual/epsilon.
constexpr double PIXEL_EPS = 0.02;   // порог отличия канала [0..1]
constexpr double FRAC_TOL = 0.01;    // допустимая доля пикселей за порогом
constexpr double MAX_CAP = 0.35;     // жёсткий кап пиковой ошибки (ловит малый грубый артефакт)

std::vector<uint8_t> render_frame(GpuContext& gpu) {
    Sprite sprite; sprite.init(gpu.device, gpu.queue);
    Renderer r;
    r.init(gpu.device, gpu.queue, sprite, WGPUTextureFormat_RGBA8Unorm, W, H);
    Scene scene;
    for (uint32_t i = 0; i < FRAME; ++i) scene.advance();
    std::vector<uint8_t> px = capture::render_offscreen(gpu.device, gpu.queue, r,
                                                        scene.snapshot((float)W / H), W, H);
    r.shutdown(); sprite.shutdown();
    return px;
}

int selftest(GpuContext& gpu) {
    std::vector<uint8_t> a = render_frame(gpu);
    std::vector<uint8_t> b = render_frame(gpu);
    if (a.empty() || b.empty()) { std::fprintf(stderr, "render failed\n"); return 1; }
    capture::DiffResult d = capture::compare(a, b, PIXEL_EPS, FRAC_TOL, MAX_CAP);
    std::printf("[golden] selftest mean=%.5f max=%.5f frac_over=%.5f -> %s\n",
                d.mean_abs, d.max_abs, d.frac_over, d.pass ? "PASS" : "FAIL");
    return d.pass ? 0 : 1;
}

int golden(GpuContext& gpu, const char* path, bool update) {
    std::vector<uint8_t> px = render_frame(gpu);
    if (px.empty()) { std::fprintf(stderr, "render failed\n"); return 1; }

    std::vector<uint8_t> ref; uint32_t rw = 0, rh = 0;
    const bool have = !update && capture::read_png(path, ref, rw, rh);
    if (!have) {
        const bool ok = capture::write_png(path, px, W, H);
        std::printf("[golden] reference %s: %s\n", update ? "updated" : "written",
                    ok ? path : "FAIL");
        return ok ? 0 : 1;
    }
    if (rw != W || rh != H) { std::fprintf(stderr, "golden size mismatch\n"); return 1; }
    capture::DiffResult d = capture::compare(px, ref, PIXEL_EPS, FRAC_TOL, MAX_CAP);
    std::printf("[golden] vs %s: mean=%.5f max=%.5f frac_over=%.5f (eps=%.3f tol=%.3f) -> %s\n",
                path, d.mean_abs, d.max_abs, d.frac_over, PIXEL_EPS, FRAC_TOL,
                d.pass ? "PASS" : "FAIL");
    return d.pass ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    GpuContext gpu;
    if (!gpu.init(nullptr)) { gpu.shutdown(); return 1; }

    int rc = 1;
    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0) {
        rc = selftest(gpu);
    } else {
        const char* path = argc >= 2 ? argv[1] : "render/golden/vertical_960x540.png";
        const bool update = argc >= 3 && std::strcmp(argv[2], "--update") == 0;
        rc = golden(gpu, path, update);
    }

    gpu.shutdown();
    return rc;
}
